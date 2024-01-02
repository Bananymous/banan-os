#pragma once

#include <BAN/Iteration.h>
#include <BAN/String.h>
#include <BAN/StringView.h>
#include <BAN/Vector.h>
#include <kernel/Credentials.h>
#include <kernel/FS/Inode.h>
#include <kernel/Memory/Heap.h>
#include <kernel/Memory/MemoryRegion.h>
#include <kernel/OpenFileDescriptorSet.h>
#include <kernel/SpinLock.h>
#include <kernel/Terminal/TTY.h>
#include <kernel/Thread.h>

#include <sys/banan-os.h>
#include <sys/mman.h>
#include <termios.h>

namespace LibELF { class LoadableELF; }

namespace Kernel
{

	class Process
	{
		BAN_NON_COPYABLE(Process);
		BAN_NON_MOVABLE(Process);

	public:
		using entry_t = Thread::entry_t;

		struct userspace_info_t
		{
			uintptr_t entry { 0 };
			int argc { 0 };
			char** argv { nullptr };
			char** envp { nullptr };
		};

	public:
		static Process* create_kernel();
		static Process* create_kernel(entry_t, void*);
		static BAN::ErrorOr<Process*> create_userspace(const Credentials&, BAN::StringView);
		~Process();
		void cleanup_function();

		void register_to_scheduler();
		void exit(int status, int signal);

		static void for_each_process(const BAN::Function<BAN::Iteration(Process&)>& callback);
		static void for_each_process_in_session(pid_t sid, const BAN::Function<BAN::Iteration(Process&)>& callback);

		void add_thread(Thread*);
		void on_thread_exit(Thread&);

		pid_t sid() const { return m_sid; }
		pid_t pgrp() const { return m_pgrp; }
		pid_t pid() const { return m_pid; }

		bool is_session_leader() const { return pid() == sid(); }

		const Credentials& credentials() const { return m_credentials; }

		BAN::ErrorOr<long> sys_exit(int status);

		BAN::ErrorOr<long> sys_gettermios(::termios*);
		BAN::ErrorOr<long> sys_settermios(const ::termios*);

		BAN::ErrorOr<long> sys_fork(uintptr_t rsp, uintptr_t rip);
		BAN::ErrorOr<long> sys_exec(BAN::StringView path, const char* const* argv, const char* const* envp);

		BAN::ErrorOr<long> sys_wait(pid_t pid, int* stat_loc, int options);
		BAN::ErrorOr<long> sys_sleep(int seconds);
		BAN::ErrorOr<long> sys_nanosleep(const timespec* rqtp, timespec* rmtp);

		BAN::ErrorOr<long> sys_setpwd(const char* path);
		BAN::ErrorOr<long> sys_getpwd(char* buffer, size_t size);

		BAN::ErrorOr<long> sys_setuid(uid_t);
		BAN::ErrorOr<long> sys_setgid(gid_t);
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

		BAN::ErrorOr<void> create_file_or_dir(BAN::StringView name, mode_t mode);
		BAN::ErrorOr<long> open_file(BAN::StringView path, int, mode_t = 0);
		BAN::ErrorOr<long> sys_open(const char* path, int, mode_t);
		BAN::ErrorOr<long> sys_openat(int, const char* path, int, mode_t);
		BAN::ErrorOr<long> sys_close(int fd);
		BAN::ErrorOr<long> sys_read(int fd, void* buffer, size_t count);
		BAN::ErrorOr<long> sys_write(int fd, const void* buffer, size_t count);
		BAN::ErrorOr<long> sys_create(const char*, mode_t);
		BAN::ErrorOr<long> sys_create_dir(const char*, mode_t);
		BAN::ErrorOr<long> sys_unlink(const char*);
		BAN::ErrorOr<long> readlink_impl(BAN::StringView absolute_path, char* buffer, size_t bufsize);
		BAN::ErrorOr<long> sys_readlink(const char* path, char* buffer, size_t bufsize);
		BAN::ErrorOr<long> sys_readlinkat(int fd, const char* path, char* buffer, size_t bufsize);

		BAN::ErrorOr<long> sys_pread(int fd, void* buffer, size_t count, off_t offset);

		BAN::ErrorOr<long> sys_chmod(const char*, mode_t);
		BAN::ErrorOr<long> sys_chown(const char*, uid_t, gid_t);

		BAN::ErrorOr<long> sys_pipe(int fildes[2]);
		BAN::ErrorOr<long> sys_dup(int fildes);
		BAN::ErrorOr<long> sys_dup2(int fildes, int fildes2);

		BAN::ErrorOr<long> sys_fcntl(int fildes, int cmd, int extra);

		BAN::ErrorOr<long> sys_seek(int fd, off_t offset, int whence);
		BAN::ErrorOr<long> sys_tell(int fd);

		BAN::ErrorOr<long> sys_fstat(int fd, struct stat*);
		BAN::ErrorOr<long> sys_fstatat(int fd, const char* path, struct stat* buf, int flag);
		BAN::ErrorOr<long> sys_stat(const char* path, struct stat* buf, int flag);

		BAN::ErrorOr<long> sys_sync(bool should_block);

		BAN::ErrorOr<long> sys_poweroff(int command);

		BAN::ErrorOr<void> mount(BAN::StringView source, BAN::StringView target);

		BAN::ErrorOr<long> sys_read_dir_entries(int fd, DirectoryEntryList* buffer, size_t buffer_size);

		BAN::ErrorOr<long> sys_mmap(const sys_mmap_t*);
		BAN::ErrorOr<long> sys_munmap(void* addr, size_t len);
		BAN::ErrorOr<long> sys_msync(void* addr, size_t len, int flags);

		BAN::ErrorOr<long> sys_tty_ctrl(int fildes, int command, int flags);

		BAN::ErrorOr<long> sys_signal(int, void (*)(int));
		static BAN::ErrorOr<long> sys_kill(pid_t pid, int signal);

		BAN::ErrorOr<long> sys_tcsetpgrp(int fd, pid_t pgid);

		BAN::ErrorOr<long> sys_termid(char*);

		BAN::ErrorOr<long> sys_clock_gettime(clockid_t, timespec*);

		TTY& tty() { ASSERT(m_controlling_terminal); return *m_controlling_terminal; }

		static Process& current() { return Thread::current().process(); }

		PageTable& page_table() { return m_page_table ? *m_page_table : PageTable::kernel(); }

		size_t proc_meminfo(off_t offset, BAN::ByteSpan) const;
		size_t proc_cmdline(off_t offset, BAN::ByteSpan) const;
		size_t proc_environ(off_t offset, BAN::ByteSpan) const;

		bool is_userspace() const { return m_is_userspace; }
		const userspace_info_t& userspace_info() const { return m_userspace_info; }

		// Returns error if page could not be allocated
		// Returns true if the page was allocated successfully
		// Return false if access was page violation (segfault)
		BAN::ErrorOr<bool> allocate_page_for_demand_paging(vaddr_t addr);

	private:
		Process(const Credentials&, pid_t pid, pid_t parent, pid_t sid, pid_t pgrp);
		static Process* create_process(const Credentials&, pid_t parent, pid_t sid = 0, pid_t pgrp = 0);

		// Load elf from a file
		static BAN::ErrorOr<BAN::UniqPtr<LibELF::LoadableELF>> load_elf_for_exec(const Credentials&, BAN::StringView file_path, const BAN::String& cwd, Kernel::PageTable&);

		int block_until_exit();

		BAN::ErrorOr<BAN::String> absolute_path_of(BAN::StringView) const;

		void validate_string_access(const char*);
		void validate_pointer_access(const void*, size_t);

	private:
		struct ExitStatus
		{
			Semaphore semaphore;
			int exit_code { 0 };
			bool exited { false };
			int waiting { 0 };
		};

		Credentials m_credentials;

		OpenFileDescriptorSet m_open_file_descriptors;

		BAN::UniqPtr<LibELF::LoadableELF> m_loadable_elf;
		BAN::Vector<BAN::UniqPtr<MemoryRegion>> m_mapped_regions;

		pid_t m_sid;
		pid_t m_pgrp;
		const pid_t m_pid;
		const pid_t m_parent;

		mutable RecursiveSpinLock m_lock;

		BAN::String m_working_directory;
		BAN::Vector<Thread*> m_threads;

		vaddr_t m_signal_handlers[_SIGMAX + 1] { };
		uint64_t m_signal_pending_mask { 0 };

		BAN::Vector<BAN::String> m_cmdline;
		BAN::Vector<BAN::String> m_environ;

		bool m_is_userspace { false };
		userspace_info_t m_userspace_info;
		ExitStatus m_exit_status;

		bool m_has_called_exec { false };

		BAN::UniqPtr<PageTable> m_page_table;
		BAN::RefPtr<TTY> m_controlling_terminal;

		friend class Thread;
	};

}