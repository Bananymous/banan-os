#include <kernel/FS/Ext2/FileSystem.h>
#include <kernel/FS/Ext2/Inode.h>

#include <BAN/HashMap.h>

#include <dirent.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

class MyBlockDevice final : public Kernel::BlockDevice
{
public:
	static constexpr size_t block_size = 1024;

public:
	static BAN::ErrorOr<BAN::RefPtr<MyBlockDevice>> from_file(const char* path, off_t offset, size_t size)
	{
		int fd = open(path, O_RDWR);
		if (fd == -1)
			return BAN::Error::from_errno(errno);

		struct stat st;
		if (fstat(fd, &st) == -1) {
			close(fd);
			return BAN::Error::from_errno(errno);
		}

		void* addr = mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, offset);
		close(fd);

		if (addr == MAP_FAILED)
			return BAN::Error::from_errno(errno);

		return BAN::RefPtr<MyBlockDevice>::adopt(new MyBlockDevice(static_cast<uint8_t*>(addr), size));
	}

	~MyBlockDevice()
	{
		munmap(m_mmap_addr, m_mmap_size);
	}

	blksize_t blksize() const override { return block_size; }

	BAN::ErrorOr<void> read_blocks(uint64_t first_block, size_t block_count, BAN::ByteSpan buffer) override
	{
		if ((first_block + block_count) * block_size > m_mmap_size)
			return BAN::Error::from_errno(EINVAL);
		memcpy(buffer.data(), m_mmap_addr + first_block * block_size, block_count * block_size);
		return {};
	}

	BAN::ErrorOr<void> write_blocks(uint64_t first_block, size_t block_count, BAN::ConstByteSpan buffer) override
	{
		if ((first_block + block_count) * block_size > m_mmap_size)
			return BAN::Error::from_errno(EINVAL);
		memcpy(m_mmap_addr + first_block * block_size, buffer.data(), block_count * block_size);
		return {};
	}

	BAN::ErrorOr<void> sync_blocks(uint64_t block, size_t block_count) override { return {}; }

private:
	MyBlockDevice(uint8_t* mmap_addr, size_t mmap_size)
		: m_mmap_addr(mmap_addr)
		, m_mmap_size(mmap_size)
	{ }

private:
	uint8_t* m_mmap_addr { nullptr };
	size_t   m_mmap_size { 0 };
};

static BAN::HashMap<ino_t, BAN::RefPtr<Kernel::Inode>> s_hardlink_map;

void copy_recursively(BAN::RefPtr<Kernel::Inode> parent, int parent_fd, dirent* dirent, int indent)
{
	struct stat st;
	if (fstatat(parent_fd, dirent->d_name, &st, AT_SYMLINK_NOFOLLOW) == -1) {
		dwarnln("failed to fstatat `{}': {}", dirent->d_name, strerror(errno));
		return;
	}

	if (auto it = s_hardlink_map.find(st.st_ino); it != s_hardlink_map.end())
	{
		if (auto ret = parent->find_inode(dirent->d_name); !ret.is_error())
		{
			if (ret.value()->ino() == it->value->ino())
				return;

			if (auto ret = parent->unlink(dirent->d_name); ret.is_error()) {
				dwarnln("failed to unlink `{}': {}", dirent->d_name, ret.error());
				return;
			}
		}

		if (auto ret = parent->link_inode(dirent->d_name, it->value); ret.is_error())
			dwarnln("failed to create hardlink for `{}': {}", dirent->d_name, ret.error());

		return;
	}

	bool should_update_data = false;

	auto inode_or_error = parent->find_inode(dirent->d_name);
	if (!inode_or_error.is_error()) {
		auto inode = inode_or_error.value();

		should_update_data =
			 inode->mtime().tv_sec  < st.st_mtim.tv_sec ||
			(inode->mtime().tv_sec == st.st_mtim.tv_sec && inode->mtime().tv_nsec < st.st_mtim.tv_nsec);

		if ((inode->mode().mode & S_IFMT) != (st.st_mode & S_IFMT)) {
			if (auto ret = parent->unlink(dirent->d_name); ret.is_error()) {
				dwarnln("failed to unlink `{}': {}", dirent->d_name, ret.error());
				return;
			}
			inode_or_error = BAN::Error::from_errno(ENOENT);
		}
	}

	if (inode_or_error.is_error()) {
		if (inode_or_error.error().get_error_code() != ENOENT) {
			dwarnln("error while looking up `{}': {}", dirent->d_name, inode_or_error.error());
			return;
		}

		auto create_result = S_ISDIR(st.st_mode)
			? parent->create_directory(dirent->d_name, st.st_mode, st.st_uid, st.st_gid)
			: parent->create_file(dirent->d_name, st.st_mode, st.st_uid, st.st_gid);

		if (create_result.is_error()) {
			dwarnln("failed to create `{}': {}", dirent->d_name, create_result.error());
			return;
		}

		inode_or_error = MUST(parent->find_inode(dirent->d_name));
		should_update_data = true;
	}

	auto inode = inode_or_error.release_value();

	ASSERT((inode->mode().mode & S_IFMT) == (st.st_mode & S_IFMT));

	if ((inode->mode().mode & 07777) != (st.st_mode & 07777) || inode->uid() != st.st_uid || inode->gid() != st.st_gid) {
		MUST(inode->chmod(st.st_mode & 07777));
		MUST(inode->chown(st.st_uid, st.st_gid));
		should_update_data = true;
	}

	switch (st.st_mode & S_IFMT)
	{
		case S_IFIFO:
		case S_IFCHR:
		case S_IFBLK:
		case S_IFSOCK:
			break;
		case S_IFLNK:
		{
			if (!should_update_data)
				break;

			size_t buffer_cap = MyBlockDevice::block_size;
			char* buffer = nullptr;

			for (;;)
			{
				char* new_buffer = static_cast<char*>(realloc(buffer, buffer_cap));
				if (new_buffer == nullptr) {
					dwarnln("failed to allocate memory for link buffer `{}': {}", dirent->d_name, strerror(errno));
					free(buffer);
					return;
				}
				buffer = new_buffer;

				const ssize_t link_len = readlinkat(parent_fd, dirent->d_name, buffer, buffer_cap);
				if (link_len == -1) {
					dwarnln("failed to readlink `{}': {}", dirent->d_name, strerror(errno));
					free(buffer);
					return;
				}

				if (link_len < buffer_cap) {
					if (auto ret = inode->set_link_target({ buffer, static_cast<size_t>(link_len) }); ret.is_error())
						dwarnln("failed to set symlink `{}' target: {}", dirent->d_name, ret.error());
					free(buffer);
					break;
				}

				buffer_cap *= 2;
			}

			break;
		}
		case S_IFREG:
		{
			if (!should_update_data)
				break;

			if (auto ret = inode->truncate(0); ret.is_error()) {
				dwarnln("failed to clear old data from `{}': {}", dirent->d_name, ret.error());
				return;
			}

			const int fd = openat(parent_fd, dirent->d_name, O_RDONLY);
			if (fd == -1) {
				dwarnln("failed to openat `{}': {}", dirent->d_name, inode_or_error.error());
				return;
			}

			const uint8_t zero_buffer[MyBlockDevice::block_size] {};
			uint8_t buffer[MyBlockDevice::block_size];

			size_t total_done = 0;
			while (total_done < st.st_size)
			{
				const ssize_t nread = read(fd, buffer, sizeof(buffer) - (total_done % sizeof(buffer)));
				if (nread == -1)
					dwarnln("failed to read from `{}': {}", dirent->d_name, strerror(errno));
				if (nread <= 0) {
					close(fd);
					return;
				}

				if (memcmp(zero_buffer, buffer, nread) == 0) {
					total_done += nread;
					continue;
				}

				size_t total_written = 0;
				while (total_written < nread)
				{
					const auto ret = inode->write(total_done + total_written, { buffer + total_written, nread - total_written });
					if (ret.is_error()) {
						dwarnln("failed to write to `{}': {}", dirent->d_name, ret.error());
						close(fd);
						return;
					}

					total_written += ret.value();
				}

				total_done += total_written;
			}

			close(fd);

			break;
		}
		case S_IFDIR:
		{
			const int fd = openat(parent_fd, dirent->d_name, O_RDONLY);
			if (fd == -1) {
				dwarnln("failed to openat `{}': {}", dirent->d_name, inode_or_error.error());
				return;
			}

			DIR* dirp = fdopendir(fd);
			if (dirp == nullptr) {
				dwarnln("failed to fdopendir `{}': {}", dirent->d_name, inode_or_error.error());
				close(fd);
				return;
			}

			while (struct dirent* dirent = readdir(dirp))
				if (strcmp(dirent->d_name, ".") != 0 && strcmp(dirent->d_name, "..") != 0)
					copy_recursively(inode, dirfd(dirp), dirent, indent + 1);

			closedir(dirp);
			break;
		}
		default:
			dwarnln("unhandled file mode {o}", st.st_mode & S_IFMT);
			ASSERT_NOT_REACHED();
	}

	if (!S_ISDIR(st.st_mode) && st.st_nlink > 1)
		MUST(s_hardlink_map.insert(st.st_ino, inode));
}

uint64_t parse_u64_or_die(const char* string)
{
	char* endp;

	errno = 0;
	const auto result = strtoull(string, &endp, 0);
	if (errno || *endp) {
		dwarnln("invalid integer `{}'", string);
		exit(1);
	}

	return result;
}

int main(int argc, char** argv)
{
	if (argc != 5) {
		dwarnln("usage: {} DISK-IMAGE EXT2-OFFSET EXT2-SIZE SYSROOT\n", argv[0]);
		return 1;
	}

	auto block_device_or_error = MyBlockDevice::from_file(argv[1], parse_u64_or_die(argv[2]), parse_u64_or_die(argv[3]));
	if (block_device_or_error.is_error()) {
		dwarnln("failed to create block device: {}", block_device_or_error.error());
		return 1;
	}

	auto block_device = block_device_or_error.release_value();
	if (auto ret = Kernel::Ext2FS::probe(block_device); ret.is_error() || !ret.value()) {
		dwarnln("file is not an ext2 filesystem");
		return 1;
	}

	auto filesystem = MUST(Kernel::Ext2FS::create(block_device));

	DIR* dirp = opendir(argv[4]);
	if (dirp == nullptr) {
		dwarnln("failed to open sysroot: {}", strerror(errno));
		return 1;
	}

	while (struct dirent* dirent = readdir(dirp))
		if (strcmp(dirent->d_name, ".") != 0 && strcmp(dirent->d_name, "..") != 0)
			copy_recursively(filesystem->root_inode(), dirfd(dirp), dirent, 0);

	closedir(dirp);

	return 0;
}
