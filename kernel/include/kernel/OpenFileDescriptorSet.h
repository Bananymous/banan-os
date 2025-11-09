#pragma once

#include <BAN/Array.h>
#include <BAN/HashSet.h>
#include <kernel/FS/Inode.h>
#include <kernel/FS/VirtualFileSystem.h>

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

		BAN::ErrorOr<int> open(VirtualFileSystem::File&&, int flags);
		BAN::ErrorOr<int> open(BAN::StringView absolute_path, int flags);

		BAN::ErrorOr<int> socket(int domain, int type, int protocol);
		BAN::ErrorOr<void> socketpair(int domain, int type, int protocol, int socket_vector[2]);

		BAN::ErrorOr<void> pipe(int fds[2]);

		BAN::ErrorOr<int> dup2(int, int);

		BAN::ErrorOr<int> fcntl(int fd, int cmd, int extra);

		BAN::ErrorOr<off_t> seek(int fd, off_t offset, int whence);
		BAN::ErrorOr<off_t> tell(int) const;

		BAN::ErrorOr<void> truncate(int fd, off_t length);

		BAN::ErrorOr<void> close(int);
		void close_all();
		void close_cloexec();

		BAN::ErrorOr<void> flock(int fd, int op);

		BAN::ErrorOr<size_t> read(int fd, BAN::ByteSpan);
		BAN::ErrorOr<size_t> write(int fd, BAN::ConstByteSpan);

		BAN::ErrorOr<size_t> read_dir_entries(int fd, struct dirent* list, size_t list_len);

		BAN::ErrorOr<size_t> recvmsg(int socket, msghdr& message, int flags);
		BAN::ErrorOr<size_t> sendmsg(int socket, const msghdr& message, int flags);

		BAN::ErrorOr<VirtualFileSystem::File> file_of(int) const;
		BAN::ErrorOr<BAN::String> path_of(int) const;
		BAN::ErrorOr<BAN::RefPtr<Inode>> inode_of(int);
		BAN::ErrorOr<int> status_flags_of(int) const;

	private:
		struct OpenFileDescription : public BAN::RefCounted<OpenFileDescription>
		{
			OpenFileDescription(VirtualFileSystem::File file, off_t offset, int status_flags)
				: file(BAN::move(file))
				, offset(offset)
				, status_flags(status_flags)
			{ }

			VirtualFileSystem::File file;
			off_t offset { 0 };
			int status_flags;

			struct flock_t
			{
				bool locked { false };
				bool shared { false };
				ThreadBlocker thread_blocker;
				BAN::HashSet<pid_t> lockers;
			};
			flock_t flock;

			friend class BAN::RefPtr<OpenFileDescription>;
		};

		struct OpenFile
		{
			OpenFile() = default;
			OpenFile(BAN::RefPtr<OpenFileDescription> description, int descriptor_flags)
				: description(BAN::move(description))
				, descriptor_flags(descriptor_flags)
			{ }

			BAN::RefPtr<Inode> inode() const { ASSERT(description); return description->file.inode; }
			BAN::StringView    path()  const { ASSERT(description); return description->file.canonical_path.sv(); }

			int& status_flags() { ASSERT(description); return description->status_flags; }
			const int& status_flags() const { ASSERT(description); return description->status_flags; }

			off_t& offset() { ASSERT(description); return description->offset; }
			const off_t& offset() const { ASSERT(description); return description->offset; }

			BAN::RefPtr<OpenFileDescription> description;
			int descriptor_flags { 0 };
		};

		BAN::ErrorOr<void> validate_fd(int) const;
		BAN::ErrorOr<int> get_free_fd() const;
		BAN::ErrorOr<void> get_free_fd_pair(int fds[2]) const;

	private:
		const Credentials& m_credentials;
		mutable Mutex m_mutex;

		BAN::Array<OpenFile, OPEN_MAX> m_open_files;
	};

}
