#pragma once

#include <BAN/Array.h>
#include <kernel/FS/Inode.h>

#include <limits.h>
#include <sys/stat.h>

namespace Kernel
{

	class OpenFileDescriptorSet
	{
		BAN_NON_COPYABLE(OpenFileDescriptorSet);

	public:
		OpenFileDescriptorSet(const Credentials&);
		~OpenFileDescriptorSet();

		OpenFileDescriptorSet& operator=(OpenFileDescriptorSet&&);

		BAN::ErrorOr<void> clone_from(const OpenFileDescriptorSet&);

		BAN::ErrorOr<int> open(BAN::StringView absolute_path, int flags);

		BAN::ErrorOr<void> pipe(int fds[2]);

		BAN::ErrorOr<int> dup(int);
		BAN::ErrorOr<int> dup2(int, int);

		BAN::ErrorOr<int> fcntl(int fd, int cmd, int extra);

		BAN::ErrorOr<void> seek(int fd, off_t offset, int whence);
		BAN::ErrorOr<off_t> tell(int) const;

		BAN::ErrorOr<void> fstat(int fd, struct stat*) const;
		BAN::ErrorOr<void> fstatat(int fd, BAN::StringView path, struct stat* buf, int flag);
		BAN::ErrorOr<void> stat(BAN::StringView absolute_path, struct stat* buf, int flag);

		BAN::ErrorOr<void> close(int);
		void close_all();
		void close_cloexec();

		BAN::ErrorOr<size_t> read(int fd, BAN::ByteSpan);
		BAN::ErrorOr<size_t> write(int fd, BAN::ConstByteSpan);

		BAN::ErrorOr<void> read_dir_entries(int fd, DirectoryEntryList* list, size_t list_size);

		BAN::ErrorOr<BAN::StringView> path_of(int) const;
		BAN::ErrorOr<BAN::RefPtr<Inode>> inode_of(int);
		BAN::ErrorOr<int> flags_of(int) const;

	private:
		struct OpenFileDescription : public BAN::RefCounted<OpenFileDescription>
		{
			OpenFileDescription(BAN::RefPtr<Inode> inode, BAN::String path, off_t offset, int flags)
				: inode(inode)
				, path(BAN::move(path))
				, offset(offset)
				, flags(flags)
			{ }

			BAN::RefPtr<Inode> inode;
			BAN::String path;
			off_t offset { 0 };
			int flags { 0 };

			friend class BAN::RefPtr<OpenFileDescription>;
		};

		BAN::ErrorOr<void> validate_fd(int) const;
		BAN::ErrorOr<int> get_free_fd() const;
		BAN::ErrorOr<void> get_free_fd_pair(int fds[2]) const;

	private:
		const Credentials& m_credentials;

		BAN::Array<BAN::RefPtr<OpenFileDescription>, OPEN_MAX> m_open_files;
	};

}
