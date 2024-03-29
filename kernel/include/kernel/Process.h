#pragma once

#include <BAN/Iteration.h>
#include <BAN/String.h>
#include <BAN/StringView.h>
#include <BAN/Vector.h>
#include <kernel/Credentials.h>
#include <kernel/FS/Inode.h>
#include <kernel/Memory/FixedWidthAllocator.h>
#include <kernel/Memory/GeneralAllocator.h>
#include <kernel/Memory/Heap.h>
#include <kernel/Memory/VirtualRange.h>
#include <kernel/OpenFileDescriptorSet.h>
#include <kernel/SpinLock.h>
#include <kernel/Terminal/TTY.h>
#include <kernel/Thread.h>

#include <termios.h>

namespace LibELF { class ELF; }

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
		static Process* create_kernel(entry_t, void*);
		static BAN::ErrorOr<Process*> create_userspace(const Credentials&, BAN::StringView);
		~Process();
		void cleanup_function();

		void exit(int status, int signal);

		static void for_each_process(const BAN::Function<BAN::Iteration(Process&)>& callback);
		static void for_each_process_in_session(pid_t sid, const BAN::Function<BAN::Iteration(Process&)>& callback);

		void add_thread(Thread*);
		void on_thread_exit(Thread&);

		pid_t sid() const { return m_sid; }
		pid_t pgrp() const { return m_pgrp; }
		pid_t pid() const { return m_pid; }

		bool is_session_leader() const { return pid() == sid(); }

		BAN::ErrorOr<long> sys_exit(int status);

		BAN::ErrorOr<long> sys_gettermios(::termios*);
		BAN::ErrorOr<long> sys_settermios(const ::termios*);

		BAN::ErrorOr<long> sys_fork(uintptr_t rsp, uintptr_t rip);
		BAN::ErrorOr<long> sys_exec(BAN::StringView path, const char* const* argv, const char* const* envp);

		BAN::ErrorOr<long> sys_wait(pid_t pid, int* stat_loc, int options);
		BAN::ErrorOr<long> sys_sleep(int seconds);
		BAN::ErrorOr<long> sys_nanosleep(const timespec* rqtp, timespec* rmtp);

		BAN::ErrorOr<long> sys_setenvp(char** envp);

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

		BAN::ErrorOr<void> create_file(BAN::StringView name, mode_t mode);
		BAN::ErrorOr<long> sys_open(BAN::StringView, int, mode_t = 0);
		BAN::ErrorOr<long> sys_openat(int, BAN::StringView, int, mode_t = 0);
		BAN::ErrorOr<long> sys_close(int fd);
		BAN::ErrorOr<long> sys_read(int fd, void* buffer, size_t count);
		BAN::ErrorOr<long> sys_write(int fd, const void* buffer, size_t count);

		BAN::ErrorOr<long> sys_pipe(int fildes[2]);
		BAN::ErrorOr<long> sys_dup(int fildes);
		BAN::ErrorOr<long> sys_dup2(int fildes, int fildes2);

		BAN::ErrorOr<long> sys_fcntl(int fildes, int cmd, int extra);

		BAN::ErrorOr<long> sys_seek(int fd, off_t offset, int whence);
		BAN::ErrorOr<long> sys_tell(int fd);

		BAN::ErrorOr<long> sys_fstat(int fd, struct stat*);
		BAN::ErrorOr<long> sys_fstatat(int fd, const char* path, struct stat* buf, int flag);
		BAN::ErrorOr<long> sys_stat(const char* path, struct stat* buf, int flag);

		BAN::ErrorOr<void> mount(BAN::StringView source, BAN::StringView target);

		BAN::ErrorOr<long> sys_read_dir_entries(int fd, DirectoryEntryList* buffer, size_t buffer_size);

		BAN::ErrorOr<long> sys_alloc(size_t);
		BAN::ErrorOr<long> sys_free(void*);

		BAN::ErrorOr<long> sys_signal(int, void (*)(int));
		BAN::ErrorOr<long> sys_raise(int signal);
		static BAN::ErrorOr<long> sys_kill(pid_t pid, int signal);

		BAN::ErrorOr<long> sys_tcsetpgrp(int fd, pid_t pgid);

		BAN::ErrorOr<long> sys_termid(char*) const;

		BAN::ErrorOr<long> sys_clock_gettime(clockid_t, timespec*) const;

		TTY& tty() { ASSERT(m_controlling_terminal); return *m_controlling_terminal; }

		static Process& current() { return Thread::current().process(); }

		PageTable& page_table() { return m_page_table ? *m_page_table : PageTable::kernel(); }

		bool is_userspace() const { return m_is_userspace; }
		const userspace_info_t& userspace_info() const { return m_userspace_info; }

	private:
		Process(const Credentials&, pid_t pid, pid_t parent, pid_t sid, pid_t pgrp);
		static Process* create_process(const Credentials&, pid_t parent, pid_t sid = 0, pid_t pgrp = 0);
		static void register_process(Process*);

		// Load an elf file to virtual address space of the current page table
		static BAN::ErrorOr<BAN::UniqPtr<LibELF::ELF>> load_elf_for_exec(const Credentials&, BAN::StringView file_path, const BAN::String& cwd, const BAN::Vector<BAN::StringView>& path_env);
		
		// Copy an elf file from the current page table to the processes own
		void load_elf_to_memory(LibELF::ELF&);

		int block_until_exit();

		BAN::ErrorOr<BAN::String> absolute_path_of(BAN::StringView) const;

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

		BAN::Vector<BAN::UniqPtr<VirtualRange>> m_mapped_ranges;

		pid_t m_sid;
		pid_t m_pgrp;
		const pid_t m_pid;
		const pid_t m_parent;

		mutable RecursiveSpinLock m_lock;

		BAN::String m_working_directory;
		BAN::Vector<Thread*> m_threads;

		BAN::Vector<BAN::UniqPtr<FixedWidthAllocator>> m_fixed_width_allocators;
		BAN::UniqPtr<GeneralAllocator> m_general_allocator;

		vaddr_t m_signal_handlers[_SIGMAX + 1] { };
		uint64_t m_signal_pending_mask { 0 };

		bool m_is_userspace { false };
		userspace_info_t m_userspace_info;
		ExitStatus m_exit_status;

		bool m_has_called_exec { false };

		BAN::UniqPtr<PageTable> m_page_table;
		BAN::RefPtr<TTY> m_controlling_terminal;

		friend class Thread;
	};

}