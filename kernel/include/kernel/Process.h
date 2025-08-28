#pragma once

#include <BAN/Atomic.h>
#include <BAN/Iteration.h>
#include <BAN/String.h>
#include <BAN/StringView.h>
#include <BAN/Vector.h>
#include <kernel/Credentials.h>
#include <kernel/ELF.h>
#include <kernel/FS/Inode.h>
#include <kernel/Lock/Mutex.h>
#include <kernel/Memory/Heap.h>
#include <kernel/Memory/MemoryRegion.h>
#include <kernel/Memory/SharedMemoryObject.h>
#include <kernel/OpenFileDescriptorSet.h>
#include <kernel/Terminal/TTY.h>
#include <kernel/Thread.h>

#include <poll.h>
#include <sys/banan-os.h>
#include <sys/mman.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/statvfs.h>
#include <sys/time.h>
#include <termios.h>

struct epoll_event;

namespace Kernel
{

	class Process
	{
		BAN_NON_COPYABLE(Process);
		BAN_NON_MOVABLE(Process);

	public:
		using entry_t = Thread::entry_t;

	public:
		static BAN::ErrorOr<Process*> create_userspace(const Credentials&, BAN::StringView path, BAN::Span<BAN::StringView> arguments);
		~Process();
		void cleanup_function(Thread*);

		void register_to_scheduler();
		void exit(int status, int signal);

		void add_thread(Thread*);
		// returns true if thread was the last one
		bool on_thread_exit(Thread&);

		pid_t sid() const { return m_sid; }
		pid_t pgrp() const { return m_pgrp; }
		pid_t pid() const { return m_pid; }

		bool is_session_leader() const { return pid() == sid(); }

		const char* name() const { return m_cmdline.empty() ? "<unknown>" : m_cmdline.front().data(); }

		const Credentials& credentials() const { return m_credentials; }

		BAN::ErrorOr<long> sys_exit(int status);

		BAN::ErrorOr<long> sys_tcgetattr(int fildes, termios*);
		BAN::ErrorOr<long> sys_tcsetattr(int fildes, int optional_actions, const termios*);

		BAN::ErrorOr<long> sys_fork(uintptr_t rsp, uintptr_t rip);
		BAN::ErrorOr<long> sys_exec(const char* path, const char* const* argv, const char* const* envp);

		BAN::ErrorOr<long> sys_wait(pid_t pid, int* stat_loc, int options);
		BAN::ErrorOr<long> sys_sleep(int seconds);
		BAN::ErrorOr<long> sys_nanosleep(const timespec* rqtp, timespec* rmtp);
		BAN::ErrorOr<long> sys_setitimer(int which, const itimerval* value, itimerval* ovalue);

		BAN::ErrorOr<long> sys_getcwd(char* buffer, size_t size);
		BAN::ErrorOr<long> sys_chdir(const char* path);
		BAN::ErrorOr<long> sys_fchdir(int fildes);
		BAN::ErrorOr<long> sys_chroot(const char* path);

		BAN::ErrorOr<long> sys_setuid(uid_t);
		BAN::ErrorOr<long> sys_setgid(gid_t);
		BAN::ErrorOr<long> sys_setsid();
		BAN::ErrorOr<long> sys_seteuid(uid_t);
		BAN::ErrorOr<long> sys_setegid(gid_t);
		BAN::ErrorOr<long> sys_setreuid(uid_t, uid_t);
		BAN::ErrorOr<long> sys_setregid(gid_t, gid_t);
		BAN::ErrorOr<long> sys_setpgid(pid_t, pid_t);

		BAN::ErrorOr<long> sys_getuid() const { return m_credentials.ruid(); }
		BAN::ErrorOr<long> sys_getgid() const { return m_credentials.rgid(); }
		BAN::ErrorOr<long> sys_geteuid() const { return m_credentials.euid(); }
		BAN::ErrorOr<long> sys_getegid() const { return m_credentials.egid(); }
		BAN::ErrorOr<long> sys_getpgid(pid_t);

		BAN::ErrorOr<long> sys_getppid() const { return m_parent; }
		BAN::ErrorOr<long> sys_getpid() const { return pid(); }

		BAN::ErrorOr<long> sys_getgroups(gid_t groups[], size_t count);
		BAN::ErrorOr<long> sys_setgroups(const gid_t groups[], size_t count);

		BAN::ErrorOr<long> open_inode(VirtualFileSystem::File&&, int flags);

		BAN::ErrorOr<void> create_file_or_dir(int fd, const char* path, mode_t mode) const;
		BAN::ErrorOr<long> sys_openat(int, const char* path, int, mode_t);
		BAN::ErrorOr<long> sys_close(int fd);
		BAN::ErrorOr<long> sys_read(int fd, void* buffer, size_t count);
		BAN::ErrorOr<long> sys_write(int fd, const void* buffer, size_t count);
		BAN::ErrorOr<long> sys_access(const char* path, int amode);
		BAN::ErrorOr<long> sys_create_dir(const char*, mode_t);
		BAN::ErrorOr<long> sys_hardlinkat(int fd1, const char* path1, int fd2, const char* path2, int flag);
		BAN::ErrorOr<long> sys_unlinkat(int fd, const char* path, int flag);
		BAN::ErrorOr<long> sys_readlinkat(int fd, const char* path, char* buffer, size_t bufsize);

		BAN::ErrorOr<long> sys_symlinkat(const char* path1, int fd, const char* path2);

		BAN::ErrorOr<long> sys_flock(int fd, int op);

		BAN::ErrorOr<long> sys_pread(int fd, void* buffer, size_t count, off_t offset);
		BAN::ErrorOr<long> sys_pwrite(int fd, const void* buffer, size_t count, off_t offset);

		BAN::ErrorOr<long> sys_fchmodat(int fd, const char* path, mode_t mode, int flag);
		BAN::ErrorOr<long> sys_fchownat(int fd, const char* path, uid_t uid, gid_t gid, int flag);
		BAN::ErrorOr<long> sys_utimensat(int fd, const char* path, const struct timespec times[2], int flag);

		BAN::ErrorOr<long> sys_socket(int domain, int type, int protocol);
		BAN::ErrorOr<long> sys_socketpair(int domain, int type, int protocol, int socket_vector[2]);
		BAN::ErrorOr<long> sys_getsockname(int socket, sockaddr* address, socklen_t* address_len);
		BAN::ErrorOr<long> sys_getpeername(int socket, sockaddr* address, socklen_t* address_len);
		BAN::ErrorOr<long> sys_getsockopt(int socket, int level, int option_name, void* option_value, socklen_t* option_len);
		BAN::ErrorOr<long> sys_setsockopt(int socket, int level, int option_name, const void* option_value, socklen_t option_len);

		BAN::ErrorOr<long> sys_accept(int socket, sockaddr* address, socklen_t* address_len, int flags);
		BAN::ErrorOr<long> sys_bind(int socket, const sockaddr* address, socklen_t address_len);
		BAN::ErrorOr<long> sys_connect(int socket, const sockaddr* address, socklen_t address_len);
		BAN::ErrorOr<long> sys_listen(int socket, int backlog);
		BAN::ErrorOr<long> sys_sendto(const sys_sendto_t*);
		BAN::ErrorOr<long> sys_recvfrom(sys_recvfrom_t*);

		BAN::ErrorOr<long> sys_ioctl(int fildes, int request, void* arg);

		BAN::ErrorOr<long> sys_pselect(sys_pselect_t* arguments);
		BAN::ErrorOr<long> sys_ppoll(pollfd* fds, nfds_t nfds, const timespec* tmp_p, const sigset_t* sigmask);

		BAN::ErrorOr<long> sys_epoll_create1(int flags);
		BAN::ErrorOr<long> sys_epoll_ctl(int epfd, int op, int fd, struct epoll_event* event);
		BAN::ErrorOr<long> sys_epoll_pwait2(int epfd, struct epoll_event* events, int maxevents, const struct timespec* timeout, const sigset_t* sigmask);

		BAN::ErrorOr<long> sys_pipe(int fildes[2]);
		BAN::ErrorOr<long> sys_dup2(int fildes, int fildes2);

		BAN::ErrorOr<long> sys_fcntl(int fildes, int cmd, int extra);

		BAN::ErrorOr<long> sys_seek(int fd, off_t offset, int whence);
		BAN::ErrorOr<long> sys_tell(int fd);

		BAN::ErrorOr<long> sys_ftruncate(int fd, off_t length);

		BAN::ErrorOr<long> sys_fsync(int fd);

		BAN::ErrorOr<long> sys_fstatat(int fd, const char* path, struct stat* buf, int flag);
		BAN::ErrorOr<long> sys_fstatvfsat(int fd, const char* path, struct statvfs* buf);

		BAN::ErrorOr<long> sys_realpath(const char* path, char* buffer);

		BAN::ErrorOr<long> sys_sync(bool should_block);

		BAN::ErrorOr<long> sys_get_nprocessor();

		static BAN::ErrorOr<long> clean_poweroff(int command);
		BAN::ErrorOr<long> sys_poweroff(int command);

		BAN::ErrorOr<long> sys_readdir(int fd, struct dirent* list, size_t list_len);

		BAN::ErrorOr<long> sys_mmap(const sys_mmap_t*);
		BAN::ErrorOr<long> sys_munmap(void* addr, size_t len);
		BAN::ErrorOr<long> sys_mprotect(void* addr, size_t len, int prot);
		BAN::ErrorOr<long> sys_msync(void* addr, size_t len, int flags);

		BAN::ErrorOr<long> sys_smo_create(size_t len, int prot);
		BAN::ErrorOr<long> sys_smo_delete(SharedMemoryObjectManager::Key);
		BAN::ErrorOr<long> sys_smo_map(SharedMemoryObjectManager::Key);

		BAN::ErrorOr<long> sys_ttyname(int fildes, char* name, size_t namesize);
		BAN::ErrorOr<long> sys_isatty(int fildes);
		BAN::ErrorOr<long> sys_posix_openpt(int flags);
		BAN::ErrorOr<long> sys_ptsname(int fildes, char* buffer, size_t buffer_len);

		BAN::ErrorOr<long> sys_tty_ctrl(int fildes, int command, int flags);

		static BAN::ErrorOr<void> kill(pid_t pid, int signal);
		BAN::ErrorOr<long> sys_kill(pid_t pid, int signal);
		BAN::ErrorOr<long> sys_sigaction(int signal, const struct sigaction* act, struct sigaction* oact);
		BAN::ErrorOr<long> sys_sigpending(sigset_t* set);
		BAN::ErrorOr<long> sys_sigprocmask(int how, const sigset_t* set, sigset_t* oset);
		BAN::ErrorOr<long> sys_sigsuspend(const sigset_t* set);
		BAN::ErrorOr<long> sys_sigwait(const sigset_t* set, int* sig);
		BAN::ErrorOr<long> sys_sigaltstack(const stack_t* ss, stack_t* oss);

		BAN::ErrorOr<long> sys_futex(int op, const uint32_t* addr, uint32_t val, const timespec* abstime);
		BAN::ErrorOr<long> sys_yield();
		BAN::ErrorOr<long> sys_set_tls(void*);
		BAN::ErrorOr<long> sys_get_tls();
		BAN::ErrorOr<long> sys_pthread_create(const pthread_attr_t* attr, void (*entry)(void*), void* arg);
		BAN::ErrorOr<long> sys_pthread_exit(void* value);
		BAN::ErrorOr<long> sys_pthread_join(pthread_t thread, void** value);
		BAN::ErrorOr<long> sys_pthread_self();
		BAN::ErrorOr<long> sys_pthread_kill(pthread_t thread, int signal);

		BAN::ErrorOr<long> sys_tcgetpgrp(int fd);
		BAN::ErrorOr<long> sys_tcsetpgrp(int fd, pid_t pgid);

		BAN::ErrorOr<long> sys_termid(char*);

		BAN::ErrorOr<long> sys_clock_gettime(clockid_t, timespec*);

		BAN::ErrorOr<long> sys_load_keymap(const char* path);

		BAN::RefPtr<TTY> controlling_terminal() { return m_controlling_terminal; }

		static Process& current() { return Thread::current().process(); }

		PageTable& page_table() { return m_page_table ? *m_page_table : PageTable::kernel(); }

		size_t proc_meminfo(off_t offset, BAN::ByteSpan) const;
		size_t proc_cmdline(off_t offset, BAN::ByteSpan) const;
		size_t proc_environ(off_t offset, BAN::ByteSpan) const;
		BAN::ErrorOr<BAN::String> proc_executable() const;

		BAN::StringView executable() const { return m_executable; }

		// Returns error if page could not be allocated
		// Returns true if the page was allocated successfully
		// Return false if access was page violation (segfault)
		BAN::ErrorOr<bool> allocate_page_for_demand_paging(vaddr_t addr, bool wants_write, bool wants_exec);

		// FIXME: remove this API
		BAN::ErrorOr<BAN::String> absolute_path_of(BAN::StringView) const;

		// ONLY CALLED BY TIMER INTERRUPT
		static void update_alarm_queue();

		const VirtualFileSystem::File& working_directory() const { return m_working_directory; }
		const VirtualFileSystem::File& root_file() const { return m_root_file; }

	private:
		Process(const Credentials&, pid_t pid, pid_t parent, pid_t sid, pid_t pgrp);
		static Process* create_process(const Credentials&, pid_t parent, pid_t sid = 0, pid_t pgrp = 0);

		struct TLSResult
		{
			BAN::UniqPtr<MemoryRegion> region;
			vaddr_t addr;
		};
		static BAN::ErrorOr<TLSResult> initialize_thread_local_storage(PageTable&, ELF::LoadResult::TLS master_tls);

		struct FileParent
		{
			VirtualFileSystem::File parent;
			BAN::StringView file_name;
		};

		BAN::ErrorOr<VirtualFileSystem::File> find_file(int fd, const char* path, int flags) const;
		BAN::ErrorOr<FileParent> find_parent_file(int fd, const char* path, int flags) const;
		BAN::ErrorOr<VirtualFileSystem::File> find_relative_parent(int fd, const char* path) const;

		BAN::ErrorOr<void> validate_string_access(const char*);
		BAN::ErrorOr<void> validate_pointer_access_check(const void*, size_t, bool needs_write);
		BAN::ErrorOr<void> validate_pointer_access(const void*, size_t, bool needs_write);
		BAN::ErrorOr<MemoryRegion*> validate_and_pin_pointer_access(const void*, size_t, bool needs_write);

		uint64_t signal_pending_mask() const
		{
			SpinLockGuard _(m_signal_lock);
			return m_signal_pending_mask;
		}

		void add_pending_signal(uint8_t signal)
		{
			ASSERT(signal >= _SIGMIN);
			ASSERT(signal <= _SIGMAX);
			ASSERT(signal < 64);
			SpinLockGuard _(m_signal_lock);
			auto handler = m_signal_handlers[signal].sa_handler;
			if (handler == SIG_IGN)
				return;
			if (handler == SIG_DFL && (signal == SIGCHLD || signal == SIGURG))
				return;
			m_signal_pending_mask |= 1ull << signal;
		}

		void remove_pending_signal(uint8_t signal)
		{
			ASSERT(signal >= _SIGMIN);
			ASSERT(signal <= _SIGMAX);
			ASSERT(signal < 64);
			SpinLockGuard _(m_signal_lock);
			m_signal_pending_mask &= ~(1ull << signal);
		}

	private:
		struct ChildExitStatus
		{
			pid_t    pid       { 0 };
			pid_t    pgrp      { 0 };
			int      exit_code { 0 };
			bool     exited    { false };
		};

		Credentials m_credentials;

		OpenFileDescriptorSet m_open_file_descriptors;

		BAN::Vector<BAN::UniqPtr<MemoryRegion>> m_mapped_regions;

		pid_t m_sid;
		pid_t m_pgrp;
		const pid_t m_pid;
		const pid_t m_parent;

		mutable Mutex m_process_lock;

		VirtualFileSystem::File m_working_directory;
		VirtualFileSystem::File m_root_file;

		BAN::Vector<Thread*> m_threads;

		struct pthread_info_t
		{
			pthread_t thread;
			void* value;
		};
		BAN::Vector<pthread_info_t> m_exited_pthreads;
		ThreadBlocker m_pthread_exit_blocker;

		uint64_t m_alarm_interval_ns { 0 };
		uint64_t m_alarm_wake_time_ns { 0 };

		mutable SpinLock m_signal_lock;
		struct sigaction m_signal_handlers[_SIGMAX + 1] { };
		uint64_t m_signal_pending_mask { 0 };

		BAN::Vector<BAN::String> m_cmdline;
		BAN::Vector<BAN::String> m_environ;
		BAN::String m_executable;

		BAN::Vector<ChildExitStatus> m_child_exit_statuses;
		ThreadBlocker m_child_exit_blocker;

		BAN::Atomic<bool> m_is_exiting { false };

		bool m_has_called_exec { false };

		BAN::UniqPtr<PageTable> m_page_table;
		BAN::RefPtr<TTY> m_controlling_terminal;

		friend class Thread;
	};

}
