#include <BAN/ScopeGuard.h>
#include <BAN/StringView.h>
#include <kernel/ACPI/ACPI.h>
#include <kernel/ELF.h>
#include <kernel/FS/DevFS/FileSystem.h>
#include <kernel/FS/ProcFS/FileSystem.h>
#include <kernel/FS/VirtualFileSystem.h>
#include <kernel/IDT.h>
#include <kernel/InterruptController.h>
#include <kernel/Lock/LockGuard.h>
#include <kernel/Memory/FileBackedRegion.h>
#include <kernel/Memory/Heap.h>
#include <kernel/Memory/MemoryBackedRegion.h>
#include <kernel/Process.h>
#include <kernel/Scheduler.h>
#include <kernel/Storage/StorageDevice.h>
#include <kernel/Terminal/PseudoTerminal.h>
#include <kernel/Timer/Timer.h>

#include <LibInput/KeyboardLayout.h>

#include <fcntl.h>
#include <stdio.h>
#include <sys/banan-os.h>
#include <sys/sysmacros.h>
#include <sys/wait.h>

namespace Kernel
{

	static BAN::LinkedList<Process*> s_alarm_processes;
	static BAN::Vector<Process*> s_processes;
	static RecursiveSpinLock s_process_lock;

	static void for_each_process(const BAN::Function<BAN::Iteration(Process&)>& callback)
	{
		SpinLockGuard _(s_process_lock);

		for (auto* process : s_processes)
		{
			auto ret = callback(*process);
			if (ret == BAN::Iteration::Break)
				return;
			ASSERT(ret == BAN::Iteration::Continue);
		}
	}

	static void for_each_process_in_session(pid_t sid, const BAN::Function<BAN::Iteration(Process&)>& callback)
	{
		SpinLockGuard _(s_process_lock);

		for (auto* process : s_processes)
		{
			if (process->sid() != sid)
				continue;
			auto ret = callback(*process);
			if (ret == BAN::Iteration::Break)
				return;
			ASSERT(ret == BAN::Iteration::Continue);
		}
	}

	Process* Process::create_process(const Credentials& credentials, pid_t parent, pid_t sid, pid_t pgrp)
	{
		static BAN::Atomic<pid_t> s_next_id = 1;

		pid_t pid = s_next_id++;
		if (sid == 0 && pgrp == 0)
		{
			sid = pid;
			pgrp = pid;
		}

		ASSERT(sid > 0);
		ASSERT(pgrp > 0);

		auto* process = new Process(credentials, pid, parent, sid, pgrp);
		ASSERT(process);

		MUST(ProcFileSystem::get().on_process_create(*process));

		return process;
	}

	void Process::register_to_scheduler()
	{
		// FIXME: Allow failing...
		{
			SpinLockGuard _(s_process_lock);
			MUST(s_processes.push_back(this));
		}
		for (auto* thread : m_threads)
			MUST(Processor::scheduler().add_thread(thread));
	}

	Process* Process::create_kernel()
	{
		auto* process = create_process({ 0, 0, 0, 0 }, 0);
		return process;
	}

	Process* Process::create_kernel(entry_t entry, void* data)
	{
		auto* process = create_process({ 0, 0, 0, 0 }, 0);
		auto* thread = MUST(Thread::create_kernel(entry, data, process));
		process->add_thread(thread);
		process->register_to_scheduler();
		return process;
	}

	BAN::ErrorOr<Process*> Process::create_userspace(const Credentials& credentials, BAN::StringView path, BAN::Span<BAN::StringView> arguments)
	{
		auto* process = create_process(credentials, 0);
		TRY(process->m_credentials.initialize_supplementary_groups());

		process->m_working_directory = VirtualFileSystem::get().root_file();
		process->m_page_table = BAN::UniqPtr<PageTable>::adopt(MUST(PageTable::create_userspace()));

		TRY(process->m_cmdline.push_back({}));
		TRY(process->m_cmdline.back().append(path));

		LockGuard _(process->m_process_lock);

		auto executable_inode = TRY(process->find_file(AT_FDCWD, path.data(), O_EXEC)).inode;
		auto executable = TRY(ELF::load_from_inode(executable_inode, process->m_credentials, process->page_table()));
		process->m_mapped_regions = BAN::move(executable.regions);

		char** argv_addr = nullptr;
		{
			size_t needed_bytes = sizeof(char*) + path.size() + 1;
			for (auto argument : arguments)
				needed_bytes += sizeof(char*) + argument.size() + 1;
			needed_bytes += sizeof(char*);

			if (auto rem = needed_bytes % PAGE_SIZE)
				needed_bytes += PAGE_SIZE - rem;

			auto argv_region = MUST(MemoryBackedRegion::create(
				process->page_table(),
				needed_bytes,
				{ .start = 0x400000, .end = KERNEL_OFFSET },
				MemoryRegion::Type::PRIVATE,
				PageTable::Flags::UserSupervisor | PageTable::Flags::Present
			));
			argv_addr = reinterpret_cast<char**>(argv_region->vaddr());

			uintptr_t offset = sizeof(char*) * (1 + arguments.size() + 1);
			for (size_t i = 0; i <= arguments.size(); i++)
			{
				const uintptr_t addr = argv_region->vaddr() + offset;
				TRY(argv_region->copy_data_to_region(i * sizeof(char*), reinterpret_cast<const uint8_t*>(&addr), sizeof(char*)));

				const auto current = (i == 0) ? path : arguments[i - 1];
				TRY(argv_region->copy_data_to_region(offset, reinterpret_cast<const uint8_t*>(current.data()), current.size()));

				const uint8_t zero = 0;
				TRY(argv_region->copy_data_to_region(offset + current.size(), &zero, 1));

				offset += current.size() + 1;
			}

			const uintptr_t zero = 0;
			TRY(argv_region->copy_data_to_region((1 + arguments.size()) * sizeof(char*), reinterpret_cast<const uint8_t*>(&zero), sizeof(char*)));

			TRY(process->m_mapped_regions.push_back(BAN::move(argv_region)));
		}

		if (executable_inode->mode().mode & +Inode::Mode::ISUID)
			process->m_credentials.set_euid(executable_inode->uid());
		if (executable_inode->mode().mode & +Inode::Mode::ISGID)
			process->m_credentials.set_egid(executable_inode->gid());

		if (executable.has_interpreter)
		{
			VirtualFileSystem::File file;
			TRY(file.canonical_path.append("<self>"));
			file.inode = executable_inode;
			process->m_userspace_info.file_fd = TRY(process->m_open_file_descriptors.open(BAN::move(file), O_RDONLY));
		}

		process->m_is_userspace = true;
		process->m_userspace_info.entry = executable.entry_point;
		process->m_userspace_info.argc = 1 + arguments.size();
		process->m_userspace_info.argv = argv_addr;
		process->m_userspace_info.envp = nullptr;

		auto* thread = MUST(Thread::create_userspace(process, process->page_table()));
		process->add_thread(thread);
		process->register_to_scheduler();
		return process;
	}

	Process::Process(const Credentials& credentials, pid_t pid, pid_t parent, pid_t sid, pid_t pgrp)
		: m_credentials(credentials)
		, m_open_file_descriptors(m_credentials)
		, m_sid(sid)
		, m_pgrp(pgrp)
		, m_pid(pid)
		, m_parent(parent)
	{
		for (size_t i = 0; i < sizeof(m_signal_handlers) / sizeof(*m_signal_handlers); i++)
		{
			m_signal_handlers[i].sa_handler = SIG_DFL;
			m_signal_handlers[i].sa_flags = 0;
		}
	}

	Process::~Process()
	{
		ASSERT(m_threads.empty());
		ASSERT(m_mapped_regions.empty());
		ASSERT(!m_page_table);
	}

	void Process::add_thread(Thread* thread)
	{
		LockGuard _(m_process_lock);
		MUST(m_threads.push_back(thread));
	}

	void Process::cleanup_function(Thread* thread)
	{
		{
			SpinLockGuard _(s_process_lock);
			for (size_t i = 0; i < s_processes.size(); i++)
			{
				if (s_processes[i] != this)
					continue;
				s_processes.remove(i);
				break;
			}
			for (auto it = s_alarm_processes.begin(); it != s_alarm_processes.end();)
			{
				if (*it == this)
					it = s_alarm_processes.remove(it);
				else
					it++;
			}
		}

		ProcFileSystem::get().on_process_delete(*this);

		m_process_lock.lock();

		m_open_file_descriptors.close_all();

		// NOTE: We must unmap ranges while the page table is still alive
		m_mapped_regions.clear();

		thread->give_keep_alive_page_table(BAN::move(m_page_table));
	}

	bool Process::on_thread_exit(Thread& thread)
	{
		LockGuard _(m_process_lock);

		ASSERT(m_threads.size() > 0);

		if (m_threads.size() == 1)
		{
			ASSERT(m_threads.front() == &thread);
			m_threads.clear();
			return true;
		}

		for (size_t i = 0; i < m_threads.size(); i++)
		{
			if (m_threads[i] == &thread)
			{
				m_threads.remove(i);
				return false;
			}
		}

		ASSERT_NOT_REACHED();
	}

	void Process::exit(int status, int signal)
	{
		if (m_parent)
		{
			for_each_process(
				[&](Process& parent) -> BAN::Iteration
				{
					if (parent.pid() != m_parent)
						return BAN::Iteration::Continue;

					for (auto& child : parent.m_child_exit_statuses)
					{
						if (child.pid != pid())
							continue;

						child.exit_code = __WGENEXITCODE(status, signal);
						child.exited = true;

						parent.add_pending_signal(SIGCHLD);
						if (!parent.m_threads.empty())
							Processor::scheduler().unblock_thread(parent.m_threads.front());

						parent.m_child_exit_blocker.unblock();

						break;
					}

					return BAN::Iteration::Break;
				}
			);
		}

		while (!m_threads.empty())
			m_threads.front()->on_exit();

		ASSERT_NOT_REACHED();
	}

	size_t Process::proc_meminfo(off_t offset, BAN::ByteSpan buffer) const
	{
		ASSERT(offset >= 0);
		if ((size_t)offset >= sizeof(proc_meminfo_t))
			return 0;

		proc_meminfo_t meminfo;
		meminfo.page_size = PAGE_SIZE;
		meminfo.virt_pages = 0;
		meminfo.phys_pages = 0;

		{
			LockGuard _(m_process_lock);
			for (auto* thread : m_threads)
			{
				meminfo.virt_pages += thread->virtual_page_count();
				meminfo.phys_pages += thread->physical_page_count();
			}
			for (auto& region : m_mapped_regions)
			{
				meminfo.virt_pages += region->virtual_page_count();
				meminfo.phys_pages += region->physical_page_count();
			}
		}

		size_t bytes = BAN::Math::min<size_t>(sizeof(proc_meminfo_t) - offset, buffer.size());
		memcpy(buffer.data(), (uint8_t*)&meminfo + offset, bytes);
		return bytes;
	}

	static size_t read_from_vec_of_str(const BAN::Vector<BAN::String>& container, size_t start, BAN::ByteSpan buffer)
	{
		size_t offset = 0;
		size_t written = 0;
		for (const auto& elem : container)
		{
			if (start < offset + elem.size() + 1)
			{
				size_t elem_offset = 0;
				if (offset < start)
					elem_offset = start - offset;

				size_t bytes = BAN::Math::min<size_t>(elem.size() + 1 - elem_offset, buffer.size() - written);
				memcpy(buffer.data() + written, elem.data() + elem_offset, bytes);

				written += bytes;
				if (written >= buffer.size())
					break;
			}
			offset += elem.size() + 1;
		}
		return written;
	}

	size_t Process::proc_cmdline(off_t offset, BAN::ByteSpan buffer) const
	{
		LockGuard _(m_process_lock);
		return read_from_vec_of_str(m_cmdline, offset, buffer);
	}

	size_t Process::proc_environ(off_t offset, BAN::ByteSpan buffer) const
	{
		LockGuard _(m_process_lock);
		return read_from_vec_of_str(m_environ, offset, buffer);
	}

	BAN::ErrorOr<VirtualFileSystem::File> Process::find_file(int fd, const char* path, int flags) const
	{
		ASSERT(m_process_lock.is_locked());

		auto parent_file = TRY(find_relative_parent(fd, path));
		auto file = path
			? TRY(VirtualFileSystem::get().file_from_relative_path(parent_file, m_credentials, path, flags))
			: BAN::move(parent_file);

		return file;
	}

	BAN::ErrorOr<Process::FileParent> Process::find_parent_file(int fd, const char* path, int flags) const
	{
		ASSERT(m_process_lock.is_locked());

		auto relative_parent = TRY(find_relative_parent(fd, path));

		VirtualFileSystem::File parent;
		BAN::StringView file_name;

		auto path_sv = path ? BAN::StringView(path) : ""_sv;
		while (!path_sv.empty() && path_sv.back() == '/')
			path_sv = path_sv.substring(0, path_sv.size() - 1);

		if (auto index = path_sv.rfind('/'); index.has_value())
		{
			parent = TRY(VirtualFileSystem::get().file_from_relative_path(relative_parent, m_credentials, path_sv.substring(0, index.value()), flags));
			file_name = path_sv.substring(index.value() + 1);
		}
		else
		{
			parent = BAN::move(relative_parent);
			file_name = path_sv;
		}

		if (!parent.inode->can_access(m_credentials, flags))
			return BAN::Error::from_errno(EACCES);
		if (!parent.inode->mode().ifdir())
			return BAN::Error::from_errno(ENOTDIR);

		while (!file_name.empty() && file_name.front() == '/')
			file_name = file_name.substring(1);
		while (!file_name.empty() && file_name.back() == '/')
			file_name = file_name.substring(0, file_name.size() - 1);

		return FileParent {
			.parent = BAN::move(parent),
			.file_name = file_name,
		};
	}

	BAN::ErrorOr<VirtualFileSystem::File> Process::find_relative_parent(int fd, const char* path) const
	{
		ASSERT(m_process_lock.is_locked());

		if (path && path[0] == '/')
			return VirtualFileSystem::get().root_file();

		if (fd == AT_FDCWD)
			return TRY(m_working_directory.clone());

		const auto status_flags = TRY(m_open_file_descriptors.status_flags_of(fd));
		if (!(status_flags & O_RDONLY) && !(status_flags & O_SEARCH))
			return BAN::Error::from_errno(EBADF);

		return TRY(m_open_file_descriptors.file_of(fd));
	}

	BAN::ErrorOr<long> Process::sys_exit(int status)
	{
		ASSERT(this == &Process::current());
		LockGuard _(m_process_lock);
		exit(status, 0);
		ASSERT_NOT_REACHED();
	}

	BAN::ErrorOr<long> Process::sys_tcgetattr(int fildes, termios* termios)
	{
		LockGuard _(m_process_lock);

		TRY(validate_pointer_access(termios, sizeof(struct termios), true));

		auto inode = TRY(m_open_file_descriptors.inode_of(fildes));
		if (!inode->is_tty())
			return BAN::Error::from_errno(ENOTTY);

		static_cast<TTY*>(inode.ptr())->get_termios(termios);

		return 0;
	}

	BAN::ErrorOr<long> Process::sys_tcsetattr(int fildes, int optional_actions, const termios* termios)
	{
		//if (optional_actions != TCSANOW)
		//	return BAN::Error::from_errno(EINVAL);
		(void)optional_actions;

		LockGuard _(m_process_lock);

		TRY(validate_pointer_access(termios, sizeof(struct termios), false));

		auto inode = TRY(m_open_file_descriptors.inode_of(fildes));
		if (!inode->is_tty())
			return BAN::Error::from_errno(ENOTTY);

		TRY(static_cast<TTY*>(inode.ptr())->set_termios(termios));

		// FIXME: SIGTTOU

		return 0;
	}

	BAN::ErrorOr<long> Process::sys_fork(uintptr_t sp, uintptr_t ip)
	{
		auto page_table = BAN::UniqPtr<PageTable>::adopt(TRY(PageTable::create_userspace()));

		LockGuard _(m_process_lock);

		ChildExitStatus* child_exit_status = nullptr;
		for (auto& child : m_child_exit_statuses)
		{
			if (child.pid != 0)
				continue;
			child_exit_status = &child;
			break;
		}
		if (child_exit_status == nullptr)
		{
			TRY(m_child_exit_statuses.emplace_back());
			child_exit_status = &m_child_exit_statuses.back();
		}

		auto working_directory = TRY(m_working_directory.clone());

		auto open_file_descriptors = TRY(BAN::UniqPtr<OpenFileDescriptorSet>::create(m_credentials));
		TRY(open_file_descriptors->clone_from(m_open_file_descriptors));

		BAN::Vector<BAN::UniqPtr<MemoryRegion>> mapped_regions;
		TRY(mapped_regions.reserve(m_mapped_regions.size()));
		for (auto& mapped_region : m_mapped_regions)
			MUST(mapped_regions.push_back(TRY(mapped_region->clone(*page_table))));

		Process* forked = create_process(m_credentials, m_pid, m_sid, m_pgrp);
		forked->m_controlling_terminal = m_controlling_terminal;
		forked->m_working_directory = BAN::move(working_directory);
		forked->m_page_table = BAN::move(page_table);
		forked->m_open_file_descriptors = BAN::move(*open_file_descriptors);
		forked->m_mapped_regions = BAN::move(mapped_regions);
		forked->m_is_userspace = m_is_userspace;
		forked->m_userspace_info = m_userspace_info;
		forked->m_has_called_exec = false;
		memcpy(forked->m_signal_handlers, m_signal_handlers, sizeof(m_signal_handlers));

		*child_exit_status = {};
		child_exit_status->pid = forked->pid();
		child_exit_status->pgrp = forked->pgrp();

		ASSERT(this == &Process::current());
		// FIXME: this should be able to fail
		Thread* thread = MUST(Thread::current().clone(forked, sp, ip));
		forked->add_thread(thread);
		forked->register_to_scheduler();

		return forked->pid();
	}

	BAN::ErrorOr<long> Process::sys_exec(const char* path, const char* const* argv, const char* const* envp)
	{
		// NOTE: We scope everything for automatic deletion
		{
			LockGuard _(m_process_lock);

			auto new_page_table = BAN::UniqPtr<PageTable>::adopt(TRY(PageTable::create_userspace()));

			TRY(validate_string_access(path));

			auto executable_file = TRY(find_file(AT_FDCWD, path, O_EXEC));
			auto executable_inode = executable_file.inode;

			BAN::Vector<BAN::String> str_argv;
			for (int i = 0; argv && argv[i]; i++)
			{
				TRY(validate_pointer_access(argv + i, sizeof(char*), false));
				TRY(validate_string_access(argv[i]));
				TRY(str_argv.emplace_back(argv[i]));
			}

			BAN::Vector<BAN::String> str_envp;
			for (int i = 0; envp && envp[i]; i++)
			{
				TRY(validate_pointer_access(envp + 1, sizeof(char*), false));
				TRY(validate_string_access(envp[i]));
				TRY(str_envp.emplace_back(envp[i]));
			}

			auto executable = TRY(ELF::load_from_inode(executable_inode, m_credentials, *new_page_table));
			auto new_mapped_regions = BAN::move(executable.regions);

			int file_fd = -1;
			if (executable.has_interpreter)
			{
				VirtualFileSystem::File file;
				file.canonical_path = BAN::move(executable_file.canonical_path);
				file.inode = executable_inode;
				file_fd = TRY(m_open_file_descriptors.open(BAN::move(file), O_RDONLY));
			}
			BAN::ScopeGuard file_closer([&] { if (file_fd != -1) MUST(m_open_file_descriptors.close(file_fd)); });

			// allocate memory on the new process for arguments and environment
			auto create_region =
				[&](BAN::Span<BAN::String> container) -> BAN::ErrorOr<BAN::UniqPtr<MemoryRegion>>
				{
					size_t bytes = sizeof(char*);
					for (auto& elem : container)
						bytes += sizeof(char*) + elem.size() + 1;

					if (auto rem = bytes % PAGE_SIZE)
						bytes += PAGE_SIZE - rem;

					auto region = TRY(MemoryBackedRegion::create(
						*new_page_table,
						bytes,
						{ .start = executable.entry_point, .end = KERNEL_OFFSET },
						MemoryRegion::Type::PRIVATE,
						PageTable::Flags::UserSupervisor | PageTable::Flags::ReadWrite | PageTable::Flags::Present
					));

					size_t data_offset = sizeof(char*) * (container.size() + 1);
					for (size_t i = 0; i < container.size(); i++)
					{
						uintptr_t ptr_addr = region->vaddr() + data_offset;
						TRY(region->copy_data_to_region(sizeof(char*) * i, (const uint8_t*)&ptr_addr, sizeof(char*)));
						TRY(region->copy_data_to_region(data_offset, (const uint8_t*)container[i].data(), container[i].size()));
						data_offset += container[i].size() + 1;
					}

					uintptr_t null = 0;
					TRY(region->copy_data_to_region(sizeof(char*) * container.size(), (const uint8_t*)&null, sizeof(char*)));

					return BAN::UniqPtr<MemoryRegion>(BAN::move(region));
				};

			TRY(new_mapped_regions.reserve(new_mapped_regions.size() + 2));
			MUST(new_mapped_regions.push_back(TRY(create_region(str_argv.span()))));
			MUST(new_mapped_regions.push_back(TRY(create_region(str_envp.span()))));

			auto* new_thread = TRY(Thread::create_userspace(this, *new_page_table));

			ASSERT(Processor::get_interrupt_state() == InterruptState::Enabled);
			Processor::set_interrupt_state(InterruptState::Disabled);

			// NOTE: bind new thread to this processor so it wont be rescheduled before end of this function
			if (auto ret = Scheduler::bind_thread_to_processor(new_thread, Processor::current_id()); ret.is_error())
			{
				Processor::set_interrupt_state(InterruptState::Enabled);
				return ret.release_error();
			}

			// after this point, everything is initialized and nothing can fail!

			ASSERT(m_threads.size() == 1);
			ASSERT(&Thread::current() == m_threads.front());

			// Make current thread standalone and terminated
			// We need to give it the current page table to keep it alive
			// while its kernel stack is in use
			m_threads.front()->m_state = Thread::State::Terminated;
			m_threads.front()->m_process = nullptr;
			m_threads.front()->give_keep_alive_page_table(BAN::move(m_page_table));

			m_threads.front() = new_thread;
			MUST(Processor::scheduler().add_thread(m_threads.front()));

			for (size_t i = 0; i < sizeof(m_signal_handlers) / sizeof(*m_signal_handlers); i++)
			{
				m_signal_handlers[i].sa_handler = SIG_DFL;
				m_signal_handlers[i].sa_flags = 0;
			}

			if (executable_inode->mode().mode & +Inode::Mode::ISUID)
				m_credentials.set_euid(executable_inode->uid());
			if (executable_inode->mode().mode & +Inode::Mode::ISGID)
				m_credentials.set_egid(executable_inode->gid());

			m_open_file_descriptors.close_cloexec();
			m_mapped_regions = BAN::move(new_mapped_regions);
			m_page_table = BAN::move(new_page_table);

			file_closer.disable();

			m_userspace_info.argc = str_argv.size();
			m_userspace_info.argv = reinterpret_cast<char**>(m_mapped_regions[m_mapped_regions.size() - 2]->vaddr());
			m_userspace_info.envp = reinterpret_cast<char**>(m_mapped_regions[m_mapped_regions.size() - 1]->vaddr());
			m_userspace_info.entry = executable.entry_point;
			m_userspace_info.file_fd = file_fd;

			m_cmdline = BAN::move(str_argv);
			m_environ = BAN::move(str_envp);
		}

		m_has_called_exec = true;

		m_threads.front()->setup_exec();
		Processor::yield();
		ASSERT_NOT_REACHED();
	}

	BAN::ErrorOr<long> Process::sys_wait(pid_t pid, int* stat_loc, int options)
	{
		if (options & ~(WCONTINUED | WNOHANG | WUNTRACED))
			return BAN::Error::from_errno(EINVAL);

		// FIXME: Add WCONTINUED and WUNTRACED when stopped/continued processes are added

		const auto pid_matches =
			[&](const ChildExitStatus& child)
			{
				if (pid == -1)
					return true;
				if (pid == 0)
					return child.pgrp == pgrp();
				if (pid < 0)
					return child.pgrp == -pid;
				return child.pid == pid;
			};

		for (;;)
		{
			pid_t exited_pid = 0;
			int exit_code = 0;
			{
				SpinLockGuard _(m_child_exit_lock);

				bool found = false;
				for (auto& child : m_child_exit_statuses)
				{
					if (!pid_matches(child))
						continue;
					found = true;
					if (!child.exited)
						continue;
					exited_pid = child.pid;
					exit_code = child.exit_code;
					child = {};
					break;
				}

				if (!found)
					return BAN::Error::from_errno(ECHILD);
			}

			if (exited_pid != 0)
			{
				if (stat_loc)
				{
					LockGuard _(m_process_lock);
					TRY(validate_pointer_access(stat_loc, sizeof(stat_loc), true));
					*stat_loc = exit_code;
				}
				remove_pending_signal(SIGCHLD);
				return exited_pid;
			}

			if (Thread::current().is_interrupted_by_signal())
				return BAN::Error::from_errno(EINTR);

			if (options & WNOHANG)
				return 0;

			m_child_exit_blocker.block_indefinite();
		}
	}

	BAN::ErrorOr<long> Process::sys_sleep(int seconds)
	{
		if (seconds == 0)
			return 0;

		const uint64_t wake_time_ms = SystemTimer::get().ms_since_boot() + (seconds * 1000);
		SystemTimer::get().sleep_ms(seconds * 1000);

		const uint64_t current_ms = SystemTimer::get().ms_since_boot();
		if (current_ms < wake_time_ms)
			return BAN::Math::div_round_up<long>(wake_time_ms - current_ms, 1000);

		return 0;
	}

	BAN::ErrorOr<long> Process::sys_nanosleep(const timespec* rqtp, timespec* rmtp)
	{
		{
			LockGuard _(m_process_lock);
			TRY(validate_pointer_access(rqtp, sizeof(timespec), false));
			if (rmtp)
				TRY(validate_pointer_access(rmtp, sizeof(timespec), true));
		}

		const uint64_t sleep_ns = (rqtp->tv_sec * 1'000'000'000) + rqtp->tv_nsec;
		if (sleep_ns == 0)
			return 0;

		const uint64_t wake_time_ns = SystemTimer::get().ns_since_boot() + sleep_ns;
		SystemTimer::get().sleep_ns(sleep_ns);

		const uint64_t current_ns = SystemTimer::get().ns_since_boot();
		if (current_ns < wake_time_ns)
		{
			if (rmtp)
			{
				const uint64_t remaining_ns = wake_time_ns - current_ns;
				rmtp->tv_sec  = remaining_ns / 1'000'000'000;
				rmtp->tv_nsec = remaining_ns % 1'000'000'000;
			}
			return BAN::Error::from_errno(EINTR);
		}

		return 0;
	}

	BAN::ErrorOr<long> Process::sys_setitimer(int which, const itimerval* value, itimerval* ovalue)
	{
		switch (which)
		{
			case ITIMER_PROF:
			case ITIMER_REAL:
			case ITIMER_VIRTUAL:
				break;
			default:
				return BAN::Error::from_errno(EINVAL);
		}

		LockGuard _(m_process_lock);

		if (value)
			TRY(validate_pointer_access(value, sizeof(itimerval), false));
		if (ovalue)
			TRY(validate_pointer_access(ovalue, sizeof(itimerval), true));

		{
			SpinLockGuard _(s_process_lock);

			const uint64_t current_ns = SystemTimer::get().ns_since_boot();

			if (m_alarm_wake_time_ns)
			{
				for (auto it = s_alarm_processes.begin(); it != s_alarm_processes.end(); it++)
				{
					if (*it != this)
						continue;
					s_alarm_processes.remove(it);
					break;
				}
			}

			if (m_alarm_wake_time_ns && ovalue)
			{
				const uint64_t interval_us = m_alarm_interval_ns / 1000;
				ovalue->it_interval = {
					.tv_sec = static_cast<time_t>(interval_us / 1'000'000),
					.tv_usec = static_cast<suseconds_t>(interval_us % 1'000'000),
				};

				const uint64_t remaining_us = current_ns < m_alarm_wake_time_ns ? (current_ns - m_alarm_wake_time_ns) / 1000 : 1;
				ovalue->it_value = {
					.tv_sec = static_cast<time_t>(remaining_us / 1'000'000),
					.tv_usec = static_cast<suseconds_t>(remaining_us % 1'000'000),
				};

				m_alarm_interval_ns = 0;
				m_alarm_wake_time_ns = 0;
			}

			if (value)
			{
				const uint64_t value_us = value->it_value.tv_sec * 1'000'000 + value->it_value.tv_usec;
				const uint64_t interval_us = value->it_interval.tv_sec * 1'000'000 + value->it_interval.tv_usec;
				if (value_us)
				{
					const uint64_t wake_time_ns = current_ns + value_us * 1000;

					auto it = s_alarm_processes.begin();
					while (it != s_alarm_processes.end() && (*it)->m_alarm_wake_time_ns < wake_time_ns)
						it++;
					TRY(s_alarm_processes.insert(it, this));

					m_alarm_wake_time_ns = wake_time_ns;
					m_alarm_interval_ns = interval_us * 1000;
				}
			}
		}

		return 0;
	}

	void Process::update_alarm_queue()
	{
		ASSERT(Processor::current_is_bsb());

		SpinLockGuard _(s_process_lock);

		const uint64_t current_ns = SystemTimer::get().ns_since_boot();

		while (!s_alarm_processes.empty())
		{
			auto* process = s_alarm_processes.front();
			if (current_ns < process->m_alarm_wake_time_ns)
				break;

			process->add_pending_signal(SIGALRM);

			ASSERT(!process->m_threads.empty());
			Processor::scheduler().unblock_thread(process->m_threads.front());

			s_alarm_processes.remove(s_alarm_processes.begin());

			if (process->m_alarm_interval_ns == 0)
				continue;

			process->m_alarm_wake_time_ns = current_ns + process->m_alarm_interval_ns;

			auto it = s_alarm_processes.begin();
			while (it != s_alarm_processes.end() && (*it)->m_alarm_wake_time_ns < process->m_alarm_wake_time_ns)
				it++;
			MUST(s_alarm_processes.insert(it, process));
		}
	}

	BAN::ErrorOr<void> Process::create_file_or_dir(int fd, const char* path, mode_t mode) const
	{
		switch (mode & Inode::Mode::TYPE_MASK)
		{
			case Inode::Mode::IFREG:
			case Inode::Mode::IFDIR:
			case Inode::Mode::IFLNK:
			case Inode::Mode::IFIFO:
			case Inode::Mode::IFSOCK:
				break;
			default:
				return BAN::Error::from_errno(ENOTSUP);
		}

		auto [parent, file_name] = TRY(find_parent_file(fd, path, O_EXEC | O_WRONLY));

		if (Inode::Mode(mode).ifdir())
			TRY(parent.inode->create_directory(file_name, mode, m_credentials.euid(), parent.inode->gid()));
		else
			TRY(parent.inode->create_file(file_name, mode, m_credentials.euid(), parent.inode->gid()));

		return {};
	}

	BAN::ErrorOr<bool> Process::allocate_page_for_demand_paging(vaddr_t address, bool wants_write)
	{
		ASSERT(&Process::current() == this);

		LockGuard _(m_process_lock);

		if (Thread::current().userspace_stack().contains(address))
		{
			TRY(Thread::current().userspace_stack().allocate_page_for_demand_paging(address));
			return true;
		}

		for (auto& region : m_mapped_regions)
		{
			if (!region->contains(address))
				continue;
			TRY(region->allocate_page_containing(address, wants_write));
			return true;
		}

		return false;
	}

	BAN::ErrorOr<long> Process::open_inode(VirtualFileSystem::File&& file, int flags)
	{
		ASSERT(file.inode);
		LockGuard _(m_process_lock);
		return TRY(m_open_file_descriptors.open(BAN::move(file), flags));
	}

	BAN::ErrorOr<long> Process::sys_openat(int fd, const char* path, int flags, mode_t mode)
	{
		if ((flags & (O_DIRECTORY | O_CREAT)) == (O_DIRECTORY | O_CREAT))
			return BAN::Error::from_errno(EINVAL);

		LockGuard _(m_process_lock);

		TRY(validate_string_access(path));

		auto [parent, file_name] = TRY(find_parent_file(fd, path, O_RDONLY));
		auto file_or_error = VirtualFileSystem::get().file_from_relative_path(parent, m_credentials, file_name, flags);

		VirtualFileSystem::File file;
		if (file_or_error.is_error())
		{
			if (!(flags & O_CREAT) || file_or_error.error().get_error_code() != ENOENT)
				return file_or_error.release_error();

			// FIXME: There is a race condition between next two lines
			TRY(parent.inode->create_file(file_name, (mode & 0777) | Inode::Mode::IFREG, m_credentials.euid(), m_credentials.egid()));
			file = TRY(VirtualFileSystem::get().file_from_relative_path(parent, m_credentials, file_name, flags & ~O_RDWR));
		}
		else
		{
			if ((flags & O_CREAT) && (flags & O_EXCL))
				return BAN::Error::from_errno(EEXIST);

			file = file_or_error.release_value();
			if (file.inode->mode().ifdir() && (flags & O_WRONLY))
				return BAN::Error::from_errno(EISDIR);
			if (!file.inode->mode().ifdir() && (flags & O_DIRECTORY))
				return BAN::Error::from_errno(ENOTDIR);
		}

		auto inode = file.inode;
		ASSERT(inode);

		fd = TRY(m_open_file_descriptors.open(BAN::move(file), flags));

		// Open controlling terminal
		if (!(flags & O_NOCTTY) && inode->is_tty() && is_session_leader() && !m_controlling_terminal)
			m_controlling_terminal = static_cast<TTY*>(inode.ptr());

		return fd;
	}

	BAN::ErrorOr<long> Process::sys_close(int fd)
	{
		LockGuard _(m_process_lock);
		TRY(m_open_file_descriptors.close(fd));
		return 0;
	}

	BAN::ErrorOr<long> Process::sys_read(int fd, void* buffer, size_t count)
	{
		LockGuard _(m_process_lock);
		TRY(validate_pointer_access(buffer, count, true));
		return TRY(m_open_file_descriptors.read(fd, BAN::ByteSpan((uint8_t*)buffer, count)));
	}

	BAN::ErrorOr<long> Process::sys_write(int fd, const void* buffer, size_t count)
	{
		LockGuard _(m_process_lock);
		TRY(validate_pointer_access(buffer, count, false));
		return TRY(m_open_file_descriptors.write(fd, BAN::ByteSpan((uint8_t*)buffer, count)));
	}

	BAN::ErrorOr<long> Process::sys_access(const char* path, int amode)
	{
		int flags = 0;
		if (amode & F_OK)
			flags |= O_SEARCH;
		if (amode & R_OK)
			flags |= O_RDONLY;
		if (amode & W_OK)
			flags |= O_WRONLY;
		if (amode & X_OK)
			flags |= O_EXEC;
		static_assert((O_RDONLY | O_WRONLY) == O_RDWR);

		LockGuard _(m_process_lock);
		TRY(validate_string_access(path));

		auto credentials = m_credentials;
		credentials.set_euid(credentials.ruid());
		credentials.set_egid(credentials.rgid());

		auto relative_parent = TRY(find_relative_parent(AT_FDCWD, path));
		TRY(VirtualFileSystem::get().file_from_relative_path(relative_parent, credentials, path, flags));

		return 0;
	}

	BAN::ErrorOr<long> Process::sys_create_dir(const char* path, mode_t mode)
	{
		LockGuard _(m_process_lock);
		TRY(validate_string_access(path));

		auto [parent, file_name] = TRY(find_parent_file(AT_FDCWD, path, O_WRONLY));
		TRY(parent.inode->create_directory(file_name, (mode & 0777) | Inode::Mode::IFDIR, m_credentials.euid(), m_credentials.egid()));

		return 0;
	}

	BAN::ErrorOr<long> Process::sys_hardlinkat(int fd1, const char* path1, int fd2, const char* path2, int flag)
	{
		LockGuard _(m_process_lock);
		TRY(validate_string_access(path1));
		TRY(validate_string_access(path2));

		auto inode = TRY(find_file(fd1, path1, flag)).inode;
		if (inode->mode().ifdir())
			return BAN::Error::from_errno(EISDIR);

		auto [parent, file_name] = TRY(find_parent_file(fd2, path2, O_WRONLY));
		if (!parent.inode->mode().ifdir())
			return BAN::Error::from_errno(ENOTDIR);

		TRY(parent.inode->link_inode(file_name, inode));

		return 0;
	}

	BAN::ErrorOr<long> Process::sys_unlink(const char* path)
	{
		LockGuard _(m_process_lock);
		TRY(validate_string_access(path));

		auto [parent, file_name] = TRY(find_parent_file(AT_FDCWD, path, O_WRONLY));
		TRY(parent.inode->unlink(file_name));

		return 0;
	}

	BAN::ErrorOr<long> Process::sys_readlinkat(int fd, const char* path, char* buffer, size_t bufsize)
	{
		LockGuard _(m_process_lock);
		TRY(validate_string_access(path));
		TRY(validate_pointer_access(buffer, bufsize, true));

		auto inode = TRY(find_file(fd, path, O_NOFOLLOW | O_RDONLY)).inode;

		// FIXME: no allocation needed
		auto link_target = TRY(inode->link_target());

		const size_t byte_count = BAN::Math::min<size_t>(link_target.size(), bufsize);
		memcpy(buffer, link_target.data(), byte_count);
		return byte_count;
	}

	BAN::ErrorOr<long> Process::sys_symlinkat(const char* path1, int fd, const char* path2)
	{
		LockGuard _(m_process_lock);
		TRY(validate_string_access(path1));
		TRY(validate_string_access(path2));

		if (!find_file(fd, path2, O_NOFOLLOW).is_error())
			return BAN::Error::from_errno(EEXIST);

		TRY(create_file_or_dir(fd, path2, 0777 | Inode::Mode::IFLNK));

		auto symlink = TRY(find_file(fd, path2, O_NOFOLLOW));
		TRY(symlink.inode->set_link_target(path1));

		return 0;
	}

	BAN::ErrorOr<long> Process::sys_pread(int fd, void* buffer, size_t count, off_t offset)
	{
		LockGuard _(m_process_lock);
		TRY(validate_pointer_access(buffer, count, true));
		auto inode = TRY(m_open_file_descriptors.inode_of(fd));
		return TRY(inode->read(offset, { (uint8_t*)buffer, count }));
	}

	BAN::ErrorOr<long> Process::sys_fchmodat(int fd, const char* path, mode_t mode, int flag)
	{
		if (mode & S_IFMASK)
			return BAN::Error::from_errno(EINVAL);
		if (flag & ~AT_SYMLINK_NOFOLLOW)
			return BAN::Error::from_errno(EINVAL);
		if (flag == AT_SYMLINK_NOFOLLOW)
			flag = O_NOFOLLOW;

		LockGuard _(m_process_lock);

		auto inode = TRY(find_file(fd, path, flag)).inode;

		if (!m_credentials.is_superuser() && inode->uid() != m_credentials.euid())
		{
			dwarnln("cannot chmod uid {} vs {}", inode->uid(), m_credentials.euid());
			return BAN::Error::from_errno(EPERM);
		}

		TRY(inode->chmod(mode));

		return 0;
	}

	BAN::ErrorOr<long> Process::sys_fchownat(int fd, const char* path, uid_t uid, gid_t gid, int flag)
	{
		if (flag & ~AT_SYMLINK_NOFOLLOW)
			return BAN::Error::from_errno(EINVAL);
		if (flag == AT_SYMLINK_NOFOLLOW)
			flag = O_NOFOLLOW;

		LockGuard _(m_process_lock);

		auto inode = TRY(find_file(fd, path, flag)).inode;

		if (uid != -1 && !m_credentials.is_superuser())
			return BAN::Error::from_errno(EPERM);
		if (gid != -1 && !m_credentials.is_superuser() && (m_credentials.euid() != uid || !m_credentials.has_egid(gid)))
			return BAN::Error::from_errno(EPERM);

		if (uid == -1)
			uid = inode->uid();
		if (gid == -1)
			gid = inode->gid();
		TRY(inode->chown(uid, gid));

		return 0;
	}

	BAN::ErrorOr<long> Process::sys_socket(int domain, int type, int protocol)
	{
		LockGuard _(m_process_lock);
		return TRY(m_open_file_descriptors.socket(domain, type, protocol));
	}

	BAN::ErrorOr<long> Process::sys_getsockname(int socket, sockaddr* address, socklen_t* address_len)
	{
		LockGuard _(m_process_lock);
		TRY(validate_pointer_access(address_len, sizeof(address_len), true));
		TRY(validate_pointer_access(address, *address_len, true));

		auto inode = TRY(m_open_file_descriptors.inode_of(socket));
		if (!inode->mode().ifsock())
			return BAN::Error::from_errno(ENOTSOCK);

		TRY(inode->getsockname(address, address_len));
		return 0;
	}

	BAN::ErrorOr<long> Process::sys_getsockopt(int socket, int level, int option_name, void* option_value, socklen_t* option_len)
	{
		LockGuard _(m_process_lock);
		TRY(validate_pointer_access(option_len, sizeof(option_len), true));
		TRY(validate_pointer_access(option_value, *option_len, true));

		auto inode = TRY(m_open_file_descriptors.inode_of(socket));
		if (!inode->mode().ifsock())
			return BAN::Error::from_errno(ENOTSOCK);

		// Because all networking is synchronous, there can not be errors to report
		if (level == SOL_SOCKET && option_name == SO_ERROR)
		{
			if (*option_len)
				*reinterpret_cast<uint8_t*>(option_value) = 0;
			*option_len = BAN::Math::min<socklen_t>(*option_len, sizeof(int));
			return 0;
		}

		return BAN::Error::from_errno(ENOTSUP);
	}

	BAN::ErrorOr<long> Process::sys_setsockopt(int socket, int level, int option_name, const void* option_value, socklen_t option_len)
	{
		LockGuard _(m_process_lock);
		TRY(validate_pointer_access(option_value, option_len, false));

		auto inode = TRY(m_open_file_descriptors.inode_of(socket));
		if (!inode->mode().ifsock())
			return BAN::Error::from_errno(ENOTSOCK);

		(void)level;
		(void)option_name;

		return BAN::Error::from_errno(ENOTSUP);
	}

	BAN::ErrorOr<long> Process::sys_accept(int socket, sockaddr* address, socklen_t* address_len, int flags)
	{
		if (address && !address_len)
			return BAN::Error::from_errno(EINVAL);
		if (!address && address_len)
			return BAN::Error::from_errno(EINVAL);
		if (flags & ~(SOCK_NONBLOCK | SOCK_CLOEXEC))
			return BAN::Error::from_errno(EINVAL);

		LockGuard _(m_process_lock);
		if (address)
		{
			TRY(validate_pointer_access(address_len, sizeof(*address_len), true));
			TRY(validate_pointer_access(address, *address_len, true));
		}

		auto inode = TRY(m_open_file_descriptors.inode_of(socket));
		if (!inode->mode().ifsock())
			return BAN::Error::from_errno(ENOTSOCK);

		int open_flags = 0;
		if (flags & SOCK_NONBLOCK)
			open_flags |= O_NONBLOCK;
		if (flags & SOCK_CLOEXEC)
			open_flags |= O_CLOEXEC;

		return TRY(inode->accept(address, address_len, open_flags));
	}

	BAN::ErrorOr<long> Process::sys_bind(int socket, const sockaddr* address, socklen_t address_len)
	{
		LockGuard _(m_process_lock);
		TRY(validate_pointer_access(address, address_len, false));

		auto inode = TRY(m_open_file_descriptors.inode_of(socket));
		if (!inode->mode().ifsock())
			return BAN::Error::from_errno(ENOTSOCK);

		TRY(inode->bind(address, address_len));
		return 0;
	}

	BAN::ErrorOr<long> Process::sys_connect(int socket, const sockaddr* address, socklen_t address_len)
	{
		LockGuard _(m_process_lock);
		TRY(validate_pointer_access(address, address_len, false));

		auto inode = TRY(m_open_file_descriptors.inode_of(socket));
		if (!inode->mode().ifsock())
			return BAN::Error::from_errno(ENOTSOCK);

		TRY(inode->connect(address, address_len));
		return 0;
	}

	BAN::ErrorOr<long> Process::sys_listen(int socket, int backlog)
	{
		LockGuard _(m_process_lock);

		auto inode = TRY(m_open_file_descriptors.inode_of(socket));
		if (!inode->mode().ifsock())
			return BAN::Error::from_errno(ENOTSOCK);

		TRY(inode->listen(backlog));
		return 0;
	}

	BAN::ErrorOr<long> Process::sys_sendto(const sys_sendto_t* arguments)
	{
		LockGuard _(m_process_lock);
		TRY(validate_pointer_access(arguments, sizeof(sys_sendto_t), false));
		TRY(validate_pointer_access(arguments->message, arguments->length, false));
		TRY(validate_pointer_access(arguments->dest_addr, arguments->dest_len, false));

		auto inode = TRY(m_open_file_descriptors.inode_of(arguments->socket));
		if (!inode->mode().ifsock())
			return BAN::Error::from_errno(ENOTSOCK);

		const auto status_flags = TRY(m_open_file_descriptors.status_flags_of(arguments->socket));
		if ((status_flags & O_NONBLOCK) && !inode->can_write())
			return BAN::Error::from_errno(EAGAIN);

		BAN::ConstByteSpan message { reinterpret_cast<const uint8_t*>(arguments->message), arguments->length };
		return TRY(inode->sendto(message, arguments->dest_addr, arguments->dest_len));
	}

	BAN::ErrorOr<long> Process::sys_recvfrom(sys_recvfrom_t* arguments)
	{
		if (arguments->address && !arguments->address_len)
			return BAN::Error::from_errno(EINVAL);
		if (!arguments->address && arguments->address_len)
			return BAN::Error::from_errno(EINVAL);

		LockGuard _(m_process_lock);
		TRY(validate_pointer_access(arguments, sizeof(sys_recvfrom_t), false));
		TRY(validate_pointer_access(arguments->buffer, arguments->length, true));
		if (arguments->address)
		{
			TRY(validate_pointer_access(arguments->address_len, sizeof(*arguments->address_len), true));
			TRY(validate_pointer_access(arguments->address, *arguments->address_len, true));
		}

		auto inode = TRY(m_open_file_descriptors.inode_of(arguments->socket));
		if (!inode->mode().ifsock())
			return BAN::Error::from_errno(ENOTSOCK);

		const auto status_flags = TRY(m_open_file_descriptors.status_flags_of(arguments->socket));
		if ((status_flags & O_NONBLOCK) && !inode->can_read())
			return BAN::Error::from_errno(EAGAIN);

		BAN::ByteSpan buffer { reinterpret_cast<uint8_t*>(arguments->buffer), arguments->length };
		return TRY(inode->recvfrom(buffer, arguments->address, arguments->address_len));
	}

	BAN::ErrorOr<long> Process::sys_ioctl(int fildes, int request, void* arg)
	{
		LockGuard _(m_process_lock);
		auto inode = TRY(m_open_file_descriptors.inode_of(fildes));
		return TRY(inode->ioctl(request, arg));
	}

	BAN::ErrorOr<long> Process::sys_pselect(sys_pselect_t* arguments)
	{
		LockGuard _(m_process_lock);

		TRY(validate_pointer_access(arguments, sizeof(sys_pselect_t), false));
		if (arguments->readfds)
			TRY(validate_pointer_access(arguments->readfds, sizeof(fd_set), true));
		if (arguments->writefds)
			TRY(validate_pointer_access(arguments->writefds, sizeof(fd_set), true));
		if (arguments->errorfds)
			TRY(validate_pointer_access(arguments->errorfds, sizeof(fd_set), true));
		if (arguments->timeout)
			TRY(validate_pointer_access(arguments->timeout, sizeof(timespec), false));
		if (arguments->sigmask)
			TRY(validate_pointer_access(arguments->sigmask, sizeof(sigset_t), false));

		const auto old_sigmask = Thread::current().m_signal_block_mask;
		if (arguments->sigmask)
			Thread::current().m_signal_block_mask = *arguments->sigmask;
		BAN::ScopeGuard sigmask_restore([old_sigmask] { Thread::current().m_signal_block_mask = old_sigmask; });

		uint64_t timedout_ns = SystemTimer::get().ns_since_boot();
		if (arguments->timeout)
		{
			timedout_ns += arguments->timeout->tv_sec * 1'000'000'000;
			timedout_ns += arguments->timeout->tv_nsec;
		}

		fd_set readfds;		FD_ZERO(&readfds);
		fd_set writefds;	FD_ZERO(&writefds);
		fd_set errorfds;	FD_ZERO(&errorfds);

		int set_bits = 0;
		for (;;)
		{
			auto update_fds =
				[&](int fd, fd_set* source, fd_set* dest, bool (Inode::*func)() const)
				{
					if (source == nullptr)
						return;

					if (!FD_ISSET(fd, source))
						return;

					auto inode_or_error = m_open_file_descriptors.inode_of(fd);
					if (inode_or_error.is_error())
						return;

					auto inode = inode_or_error.release_value();
					if ((inode.ptr()->*func)())
					{
						FD_SET(fd, dest);
						set_bits++;
					}
				};

			for (int i = 0; i < arguments->nfds; i++)
			{
				update_fds(i, arguments->readfds, &readfds, &Inode::can_read);
				update_fds(i, arguments->writefds, &writefds, &Inode::can_write);
				update_fds(i, arguments->errorfds, &errorfds, &Inode::has_error);
			}

			if (set_bits > 0)
				break;

			if (arguments->timeout && SystemTimer::get().ns_since_boot() >= timedout_ns)
				break;

			LockFreeGuard free(m_process_lock);
			TRY(Thread::current().sleep_or_eintr_ms(1));
		}

		if (arguments->readfds)
			FD_ZERO(arguments->readfds);
		if (arguments->writefds)
			FD_ZERO(arguments->writefds);
		if (arguments->errorfds)
			FD_ZERO(arguments->errorfds);

		for (int i = 0; i < arguments->nfds; i++)
		{
			if (arguments->readfds && FD_ISSET(i, &readfds))
				FD_SET(i, arguments->readfds);
			if (arguments->writefds && FD_ISSET(i, &writefds))
				FD_SET(i, arguments->writefds);
			if (arguments->errorfds && FD_ISSET(i, &errorfds))
				FD_SET(i, arguments->errorfds);
		}

		return set_bits;
	}

	BAN::ErrorOr<long> Process::sys_pipe(int fildes[2])
	{
		LockGuard _(m_process_lock);
		TRY(validate_pointer_access(fildes, sizeof(int) * 2, true));
		TRY(m_open_file_descriptors.pipe(fildes));
		return 0;
	}

	BAN::ErrorOr<long> Process::sys_dup2(int fildes, int fildes2)
	{
		LockGuard _(m_process_lock);
		return TRY(m_open_file_descriptors.dup2(fildes, fildes2));
	}

	BAN::ErrorOr<long> Process::sys_fcntl(int fildes, int cmd, int extra)
	{
		LockGuard _(m_process_lock);
		return TRY(m_open_file_descriptors.fcntl(fildes, cmd, extra));
	}

	BAN::ErrorOr<long> Process::sys_seek(int fd, off_t offset, int whence)
	{
		LockGuard _(m_process_lock);
		return TRY(m_open_file_descriptors.seek(fd, offset, whence));
	}

	BAN::ErrorOr<long> Process::sys_tell(int fd)
	{
		LockGuard _(m_process_lock);
		return TRY(m_open_file_descriptors.tell(fd));
	}

	BAN::ErrorOr<long> Process::sys_truncate(int fd, off_t length)
	{
		LockGuard _(m_process_lock);
		TRY(m_open_file_descriptors.truncate(fd, length));
		return 0;
	}

	BAN::ErrorOr<long> Process::sys_fsync(int fd)
	{
		LockGuard _(m_process_lock);
		auto inode = TRY(m_open_file_descriptors.inode_of(fd));
		TRY(inode->fsync());
		return 0;
	}

	BAN::ErrorOr<long> Process::sys_fstatat(int fd, const char* path, struct stat* buf, int flag)
	{
		if (flag & ~AT_SYMLINK_NOFOLLOW)
			return BAN::Error::from_errno(EINVAL);
		if (flag == AT_SYMLINK_NOFOLLOW)
			flag = O_NOFOLLOW;

		LockGuard _(m_process_lock);
		TRY(validate_pointer_access(buf, sizeof(struct stat), true));

		auto inode = TRY(find_file(fd, path, flag)).inode;
		buf->st_dev		= inode->dev();
		buf->st_ino		= inode->ino();
		buf->st_mode	= inode->mode().mode;
		buf->st_nlink	= inode->nlink();
		buf->st_uid		= inode->uid();
		buf->st_gid		= inode->gid();
		buf->st_rdev	= inode->rdev();
		buf->st_size	= inode->size();
		buf->st_atim	= inode->atime();
		buf->st_mtim	= inode->mtime();
		buf->st_ctim	= inode->ctime();
		buf->st_blksize	= inode->blksize();
		buf->st_blocks	= inode->blocks();

		return 0;
	}

	BAN::ErrorOr<long> Process::sys_fstatvfsat(int fd, const char* path, struct statvfs* buf)
	{
		LockGuard _(m_process_lock);
		TRY(validate_pointer_access(buf, sizeof(struct statvfs), true));

		auto inode = TRY(find_file(fd, path, 0)).inode;
		auto* fs = inode->filesystem();
		if (fs == nullptr)
		{
			ASSERT(inode->mode().ifsock() || inode->mode().ififo());
			dwarnln("TODO: fstatvfs on sockets or pipe?");
			return BAN::Error::from_errno(EINVAL);
		}

		buf->f_bsize   = fs->bsize();
		buf->f_frsize  = fs->frsize();
		buf->f_blocks  = fs->blocks();
		buf->f_bfree   = fs->bfree();
		buf->f_bavail  = fs->bavail();
		buf->f_files   = fs->files();
		buf->f_ffree   = fs->ffree();
		buf->f_favail  = fs->favail();
		buf->f_fsid    = fs->fsid();
		buf->f_flag    = fs->flag();
		buf->f_namemax = fs->namemax();

		return 0;
	}

	BAN::ErrorOr<long> Process::sys_realpath(const char* path, char* buffer)
	{
		LockGuard _(m_process_lock);
		TRY(validate_string_access(path));
		TRY(validate_pointer_access(buffer, PATH_MAX, true));

		auto file = TRY(find_file(AT_FDCWD, path, O_RDONLY));
		if (file.canonical_path.size() >= PATH_MAX)
			return BAN::Error::from_errno(ENAMETOOLONG);

		strcpy(buffer, file.canonical_path.data());
		return file.canonical_path.size();
	}

	BAN::ErrorOr<long> Process::sys_sync(bool should_block)
	{
		DevFileSystem::get().initiate_sync(should_block);
		return 0;
	}

	[[noreturn]] static void reset_system()
	{
		(void)ACPI::ACPI::get().reset();

		dwarnln("Could not reset with ACPI, crashing the cpu");

		// reset through triple fault
		IDT::force_triple_fault();
	}

	BAN::ErrorOr<long> Process::clean_poweroff(int command)
	{
		if (command != POWEROFF_REBOOT && command != POWEROFF_SHUTDOWN)
			return BAN::Error::from_errno(EINVAL);

		// FIXME: gracefully kill all processes

		DevFileSystem::get().initiate_sync(true);

		if (command == POWEROFF_REBOOT)
			reset_system();

		TRY(ACPI::ACPI::get().poweroff());
		ASSERT_NOT_REACHED();
	}

	BAN::ErrorOr<long> Process::sys_poweroff(int command)
	{
		return clean_poweroff(command);
	}

	BAN::ErrorOr<long> Process::sys_readdir(int fd, struct dirent* list, size_t list_len)
	{
		LockGuard _(m_process_lock);
		TRY(validate_pointer_access(list, sizeof(dirent) * list_len, true));
		return TRY(m_open_file_descriptors.read_dir_entries(fd, list, list_len));
	}

	BAN::ErrorOr<long> Process::sys_setpwd(const char* path)
	{
		LockGuard _(m_process_lock);

		auto file = TRY(find_file(AT_FDCWD, path, O_SEARCH));
		m_working_directory = BAN::move(file);

		return 0;
	}

	BAN::ErrorOr<long> Process::sys_getpwd(char* buffer, size_t size)
	{
		LockGuard _(m_process_lock);

		TRY(validate_pointer_access(buffer, size, true));

		if (size < m_working_directory.canonical_path.size() + 1)
			return BAN::Error::from_errno(ERANGE);

		memcpy(buffer, m_working_directory.canonical_path.data(), m_working_directory.canonical_path.size());
		buffer[m_working_directory.canonical_path.size()] = '\0';

		return (long)buffer;
	}

	BAN::ErrorOr<long> Process::sys_mmap(const sys_mmap_t* args)
	{
		{
			LockGuard _(m_process_lock);
			TRY(validate_pointer_access(args, sizeof(sys_mmap_t), true));
		}

		if (args->prot != PROT_NONE && (args->prot & ~(PROT_READ | PROT_WRITE | PROT_EXEC)))
			return BAN::Error::from_errno(EINVAL);

		if (!(args->flags & MAP_PRIVATE) == !(args->flags & MAP_SHARED))
			return BAN::Error::from_errno(EINVAL);
		auto region_type = (args->flags & MAP_PRIVATE) ? MemoryRegion::Type::PRIVATE : MemoryRegion::Type::SHARED;

		PageTable::flags_t page_flags = 0;
		if (args->prot & PROT_READ)
			page_flags |= PageTable::Flags::Present;
		if (args->prot & PROT_WRITE)
			page_flags |= PageTable::Flags::ReadWrite | PageTable::Flags::Present;
		if (args->prot & PROT_EXEC)
			page_flags |= PageTable::Flags::Execute | PageTable::Flags::Present;

		if (page_flags == 0)
			page_flags = PageTable::Flags::Reserved;
		else
			page_flags |= PageTable::Flags::UserSupervisor;

		AddressRange address_range { .start = 0x400000, .end = KERNEL_OFFSET };
		if (args->flags & MAP_FIXED)
		{
			vaddr_t base_addr = reinterpret_cast<vaddr_t>(args->addr);
			address_range.start = BAN::Math::div_round_up<vaddr_t>(base_addr, PAGE_SIZE) * PAGE_SIZE;
			address_range.end = BAN::Math::div_round_up<vaddr_t>(base_addr + args->len, PAGE_SIZE) * PAGE_SIZE;
		}

		if (args->flags & MAP_ANONYMOUS)
		{
			if (args->off != 0)
				return BAN::Error::from_errno(EINVAL);

			auto region = TRY(MemoryBackedRegion::create(
				page_table(),
				args->len,
				address_range,
				region_type, page_flags
			));

			LockGuard _(m_process_lock);
			TRY(m_mapped_regions.push_back(BAN::move(region)));
			return m_mapped_regions.back()->vaddr();
		}

		LockGuard _(m_process_lock);

		auto inode = TRY(m_open_file_descriptors.inode_of(args->fildes));

		const auto status_flags = TRY(m_open_file_descriptors.status_flags_of(args->fildes));
		if (!(status_flags & O_RDONLY))
			return BAN::Error::from_errno(EACCES);
		if (region_type == MemoryRegion::Type::SHARED)
			if ((args->prot & PROT_WRITE) && !(status_flags & O_WRONLY))
				return BAN::Error::from_errno(EACCES);

		BAN::UniqPtr<MemoryRegion> memory_region;
		if (inode->mode().ifreg())
		{
			memory_region = TRY(FileBackedRegion::create(
				inode,
				page_table(),
				args->off, args->len,
				address_range,
				region_type, page_flags
			));
		}
		else if (inode->is_device())
		{
			memory_region = TRY(static_cast<Device&>(*inode).mmap_region(
				page_table(),
				args->off, args->len,
				address_range,
				region_type, page_flags
			));
		}

		if (!memory_region)
			return BAN::Error::from_errno(ENODEV);

		TRY(m_mapped_regions.push_back(BAN::move(memory_region)));
		return m_mapped_regions.back()->vaddr();
	}

	BAN::ErrorOr<long> Process::sys_munmap(void* addr, size_t len)
	{
		if (len == 0)
			return BAN::Error::from_errno(EINVAL);

		vaddr_t vaddr = (vaddr_t)addr;
		if (vaddr % PAGE_SIZE != 0)
			return BAN::Error::from_errno(EINVAL);

		LockGuard _(m_process_lock);

		// FIXME: We should only map partial regions
		for (size_t i = 0; i < m_mapped_regions.size(); i++)
			if (m_mapped_regions[i]->overlaps(vaddr, len))
				m_mapped_regions.remove(i);

		return 0;
	}

	BAN::ErrorOr<long> Process::sys_msync(void* addr, size_t len, int flags)
	{
		if (flags != MS_SYNC && flags != MS_ASYNC && flags != MS_INVALIDATE)
			return BAN::Error::from_errno(EINVAL);

		LockGuard _(m_process_lock);

		const vaddr_t vaddr = reinterpret_cast<vaddr_t>(addr);
		for (auto& mapped_region : m_mapped_regions)
			if (mapped_region->overlaps(vaddr, len))
				TRY(mapped_region->msync(vaddr, len, flags));

		return 0;
	}

	BAN::ErrorOr<long> Process::sys_smo_create(size_t len, int prot)
	{
		if (len == 0)
			return BAN::Error::from_errno(EINVAL);
		if (prot & ~(PROT_READ | PROT_WRITE | PROT_EXEC | PROT_NONE))
			return BAN::Error::from_errno(EINVAL);

		if (auto rem = len % PAGE_SIZE)
			len += PAGE_SIZE - rem;

		PageTable::flags_t page_flags = 0;
		if (prot & PROT_READ)
			page_flags |= PageTable::Flags::Present;
		if (prot & PROT_WRITE)
			page_flags |= PageTable::Flags::ReadWrite | PageTable::Flags::Present;
		if (prot & PROT_EXEC)
			page_flags |= PageTable::Flags::Execute | PageTable::Flags::Present;

		if (page_flags == 0)
			page_flags |= PageTable::Flags::Reserved;
		else
			page_flags |= PageTable::Flags::UserSupervisor;

		return TRY(SharedMemoryObjectManager::get().create_object(len, page_flags));
	}

	BAN::ErrorOr<long> Process::sys_smo_delete(SharedMemoryObjectManager::Key key)
	{
		TRY(SharedMemoryObjectManager::get().delete_object(key));
		return 0;
	}

	BAN::ErrorOr<long> Process::sys_smo_map(SharedMemoryObjectManager::Key key)
	{
		auto region = TRY(SharedMemoryObjectManager::get().map_object(key, page_table(), { .start = 0x400000, .end = KERNEL_OFFSET }));

		LockGuard _(m_process_lock);
		TRY(m_mapped_regions.push_back(BAN::move(region)));
		return m_mapped_regions.back()->vaddr();
	}

	BAN::ErrorOr<long> Process::sys_ttyname(int fildes, char* storage)
	{
		LockGuard _(m_process_lock);
		TRY(validate_pointer_access(storage, TTY_NAME_MAX, true));
		auto inode = TRY(m_open_file_descriptors.inode_of(fildes));
		if (!inode->is_tty())
			return BAN::Error::from_errno(ENOTTY);
		auto path = TRY(m_open_file_descriptors.path_of(fildes));
		ASSERT(path.size() < TTY_NAME_MAX);
		strncpy(storage, path.data(), path.size());
		storage[path.size()] = '\0';
		return 0;
	}

	BAN::ErrorOr<long> Process::sys_isatty(int fildes)
	{
		LockGuard _(m_process_lock);
		auto inode = TRY(m_open_file_descriptors.inode_of(fildes));
		if (!inode->is_tty())
			return BAN::Error::from_errno(ENOTTY);
		return 0;
	}

	BAN::ErrorOr<long> Process::sys_posix_openpt(int flags)
	{
		if (flags & ~(O_RDWR | O_NOCTTY))
			return BAN::Error::from_errno(EINVAL);

		mode_t mode = 0440;
		if (flags & O_WRONLY)
			mode = 0660;

		auto pts_master = TRY(PseudoTerminalMaster::create(mode, m_credentials.ruid(), m_credentials.rgid()));
		auto pts_slave = TRY(pts_master->slave());

		VirtualFileSystem::File file;
		file.inode = pts_master;
		TRY(file.canonical_path.append(pts_master->name()));

		LockGuard _(m_process_lock);

		int pts_master_fd = TRY(m_open_file_descriptors.open(BAN::move(file), flags));

		if (!(flags & O_NOCTTY) && is_session_leader() && !m_controlling_terminal)
			m_controlling_terminal = (TTY*)pts_slave.ptr();

		return pts_master_fd;
	}

	BAN::ErrorOr<long> Process::sys_ptsname(int fildes, char* buffer, size_t buffer_len)
	{
		LockGuard _(m_process_lock);
		TRY(validate_pointer_access(buffer, buffer_len, true));

		if (TRY(m_open_file_descriptors.path_of(fildes)) != "<ptmx>"_sv)
			return BAN::Error::from_errno(ENOTTY);

		auto inode = TRY(m_open_file_descriptors.inode_of(fildes));
		auto ptsname = TRY(static_cast<PseudoTerminalMaster*>(inode.ptr())->ptsname());

		const size_t to_copy = BAN::Math::min(ptsname.size(), buffer_len - 1);
		memcpy(buffer, ptsname.data(), to_copy);
		buffer[to_copy] = '\0';

		return 0;
	}

	BAN::ErrorOr<long> Process::sys_tty_ctrl(int fildes, int command, int flags)
	{
		LockGuard _(m_process_lock);

		auto inode = TRY(m_open_file_descriptors.inode_of(fildes));
		if (!inode->is_tty())
			return BAN::Error::from_errno(ENOTTY);

		TRY(((TTY*)inode.ptr())->tty_ctrl(command, flags));

		return 0;
	}

	BAN::ErrorOr<long> Process::sys_termid(char* buffer)
	{
		LockGuard _(m_process_lock);

		TRY(validate_string_access(buffer));

		auto& tty = m_controlling_terminal;

		if (!tty)
			buffer[0] = '\0';
		else
		{
			ASSERT(minor(tty->rdev()) < 10);
			strcpy(buffer, "/dev/tty0");
			buffer[8] += minor(tty->rdev());
		}

		return 0;
	}

	BAN::ErrorOr<long> Process::sys_clock_gettime(clockid_t clock_id, timespec* tp)
	{
		{
			LockGuard _(m_process_lock);
			TRY(validate_pointer_access(tp, sizeof(timespec), true));
		}

		switch (clock_id)
		{
			case CLOCK_MONOTONIC:
				*tp = SystemTimer::get().time_since_boot();
				break;
			case CLOCK_REALTIME:
				*tp = SystemTimer::get().real_time();
				break;
			default:
				dwarnln("TODO: clock_gettime({})", clock_id);
				return BAN::Error::from_errno(ENOTSUP);
		}
		return 0;
	}


	BAN::ErrorOr<long> Process::sys_load_keymap(const char* path)
	{
		LockGuard _(m_process_lock);
		TRY(validate_string_access(path));

		if (!m_credentials.is_superuser())
			return BAN::Error::from_errno(EPERM);

		auto absolute_path = TRY(absolute_path_of(path));
		TRY(LibInput::KeyboardLayout::get().load_from_file(absolute_path));
		return 0;
	}

	BAN::ErrorOr<void> Process::kill(pid_t pid, int signal)
	{
		if (pid == 0 || pid == -1)
			return BAN::Error::from_errno(ENOTSUP);
		if (signal != 0 && (signal < _SIGMIN || signal > _SIGMAX))
			return BAN::Error::from_errno(EINVAL);

		bool found = false;
		for_each_process(
			[&](Process& process)
			{
				if (pid == process.pid() || -pid == process.pgrp())
				{
					found = true;
					if (signal)
					{
						process.add_pending_signal(signal);
						if (!process.m_threads.empty())
							Processor::scheduler().unblock_thread(process.m_threads.front());
					}
					return (pid > 0) ? BAN::Iteration::Break : BAN::Iteration::Continue;
				}
				return BAN::Iteration::Continue;
			}
		);

		if (found)
			return {};
		return BAN::Error::from_errno(ESRCH);
	}

	BAN::ErrorOr<long> Process::sys_kill(pid_t pid, int signal)
	{
		if (pid == 0 || pid == -1)
			return BAN::Error::from_errno(ENOTSUP);
		if (signal != 0 && (signal < _SIGMIN || signal > _SIGMAX))
			return BAN::Error::from_errno(EINVAL);

		if (pid == m_pid)
		{
			if (signal)
				add_pending_signal(signal);
			return 0;
		}

		TRY(kill(pid, signal));
		return 0;
	}

	BAN::ErrorOr<long> Process::sys_sigaction(int signal, const struct sigaction* act, struct sigaction* oact)
	{
		if (signal < _SIGMIN || signal > _SIGMAX)
			return BAN::Error::from_errno(EINVAL);

		LockGuard _(m_process_lock);
		if (act)
			TRY(validate_pointer_access(act, sizeof(struct sigaction), false));
		if (oact)
			TRY(validate_pointer_access(oact, sizeof(struct sigaction), true));

		SpinLockGuard signal_lock_guard(m_signal_lock);

		if (oact)
			*oact = m_signal_handlers[signal];

		if (act)
		{
			if (act->sa_flags == SA_RESTART)
				dwarnln("FIXME: sigaction ignoring SA_RESTART", signal, act->sa_flags);
			else if (act->sa_flags)
			{
				dwarnln("TODO: sigaction({}, {H})", signal, act->sa_flags);
				return BAN::Error::from_errno(ENOTSUP);
			}
			m_signal_handlers[signal] = *act;
		}

		return 0;
	}

	BAN::ErrorOr<long> Process::sys_sigpending(sigset_t* set)
	{
		LockGuard _(m_process_lock);
		TRY(validate_pointer_access(set, sizeof(sigset_t), true));
		*set = (signal_pending_mask() | Thread::current().m_signal_pending_mask) & Thread::current().m_signal_block_mask;
		return 0;
	}

	BAN::ErrorOr<long> Process::sys_sigprocmask(int how, const sigset_t* set, sigset_t* oset)
	{
		LockGuard _(m_process_lock);
		if (set)
			TRY(validate_pointer_access(set, sizeof(sigset_t), false));
		if (oset)
			TRY(validate_pointer_access(oset, sizeof(sigset_t), true));

		if (oset)
			*oset = Thread::current().m_signal_block_mask;

		if (set)
		{
			const sigset_t mask = *set & ~(SIGKILL | SIGSTOP);
			switch (how)
			{
				case SIG_BLOCK:
					Thread::current().m_signal_block_mask |= mask;
					break;
				case SIG_SETMASK:
					Thread::current().m_signal_block_mask  = mask;
					break;
				case SIG_UNBLOCK:
					Thread::current().m_signal_block_mask &= ~mask;
					break;
				default:
					return BAN::Error::from_errno(EINVAL);
			}
		}

		return 0;
	}

	BAN::ErrorOr<long> Process::sys_tcgetpgrp(int fd)
	{
		LockGuard _(m_process_lock);

		if (!m_controlling_terminal)
			return BAN::Error::from_errno(ENOTTY);

		auto inode = TRY(m_open_file_descriptors.inode_of(fd));
		if (!inode->is_tty())
			return BAN::Error::from_errno(ENOTTY);

		auto* tty = static_cast<TTY*>(inode.ptr());
		if (tty != m_controlling_terminal.ptr())
			return BAN::Error::from_errno(ENOTTY);

		return tty->foreground_pgrp();
	}

	BAN::ErrorOr<long> Process::sys_tcsetpgrp(int fd, pid_t pgrp)
	{
		LockGuard _(m_process_lock);

		if (!m_controlling_terminal)
			return BAN::Error::from_errno(ENOTTY);

		bool valid_pgrp = false;
		for_each_process(
			[&](Process& process)
			{
				if (process.sid() == sid() && process.pgrp() == pgrp)
				{
					valid_pgrp = true;
					return BAN::Iteration::Break;
				}
				return BAN::Iteration::Continue;
			}
		);
		if (!valid_pgrp)
			return BAN::Error::from_errno(EPERM);

		auto inode = TRY(m_open_file_descriptors.inode_of(fd));
		if (!inode->is_tty())
			return BAN::Error::from_errno(ENOTTY);

		auto* tty = static_cast<TTY*>(inode.ptr());
		if (tty != m_controlling_terminal.ptr())
			return BAN::Error::from_errno(ENOTTY);

		tty->set_foreground_pgrp(pgrp);
		return 0;
	}

	BAN::ErrorOr<long> Process::sys_setuid(uid_t uid)
	{
		if (uid < 0 || uid >= 1'000'000'000)
			return BAN::Error::from_errno(EINVAL);

		LockGuard _(m_process_lock);

		// If the process has appropriate privileges, setuid() shall set the real user ID, effective user ID, and the saved
		// set-user-ID of the calling process to uid.
		if (m_credentials.is_superuser())
		{
			m_credentials.set_euid(uid);
			m_credentials.set_ruid(uid);
			m_credentials.set_suid(uid);
			TRY(m_credentials.initialize_supplementary_groups());
			return 0;
		}

		// If the process does not have appropriate privileges, but uid is equal to the real user ID or the saved set-user-ID,
		// setuid() shall set the effective user ID to uid; the real user ID and saved set-user-ID shall remain unchanged.
		if (uid == m_credentials.ruid() || uid == m_credentials.suid())
		{
			m_credentials.set_euid(uid);
			TRY(m_credentials.initialize_supplementary_groups());
			return 0;
		}

		return BAN::Error::from_errno(EPERM);
	}

	BAN::ErrorOr<long> Process::sys_setgid(gid_t gid)
	{
		if (gid < 0 || gid >= 1'000'000'000)
			return BAN::Error::from_errno(EINVAL);

		LockGuard _(m_process_lock);

		// If the process has appropriate privileges, setgid() shall set the real group ID, effective group ID, and the saved
		// set-group-ID of the calling process to gid.
		if (m_credentials.is_superuser())
		{
			m_credentials.set_egid(gid);
			m_credentials.set_rgid(gid);
			m_credentials.set_sgid(gid);
			return 0;
		}

		// If the process does not have appropriate privileges, but gid is equal to the real group ID or the saved set-group-ID,
		// setgid() shall set the effective group ID to gid; the real group ID and saved set-group-ID shall remain unchanged.
		if (gid == m_credentials.rgid() || gid == m_credentials.sgid())
		{
			m_credentials.set_egid(gid);
			return 0;
		}

		return BAN::Error::from_errno(EPERM);
	}

	BAN::ErrorOr<long> Process::sys_setsid()
	{
		LockGuard _(m_process_lock);

		if (is_session_leader() || m_pid == m_pgrp)
			return BAN::Error::from_errno(EPERM);

		m_sid = m_pid;
		m_pgrp = m_pid;
		m_controlling_terminal.clear();

		return 0;
	}

	BAN::ErrorOr<long> Process::sys_seteuid(uid_t uid)
	{
		if (uid < 0 || uid >= 1'000'000'000)
			return BAN::Error::from_errno(EINVAL);

		LockGuard _(m_process_lock);

		// If uid is equal to the real user ID or the saved set-user-ID, or if the process has appropriate privileges, seteuid()
		// shall set the effective user ID of the calling process to uid; the real user ID and saved set-user-ID shall remain unchanged.
		if (uid == m_credentials.ruid() || uid == m_credentials.suid() || m_credentials.is_superuser())
		{
			m_credentials.set_euid(uid);
			TRY(m_credentials.initialize_supplementary_groups());
			return 0;
		}

		return BAN::Error::from_errno(EPERM);
	}

	BAN::ErrorOr<long> Process::sys_setegid(gid_t gid)
	{
		if (gid < 0 || gid >= 1'000'000'000)
			return BAN::Error::from_errno(EINVAL);

		LockGuard _(m_process_lock);

		// If gid is equal to the real group ID or the saved set-group-ID, or if the process has appropriate privileges, setegid()
		// shall set the effective group ID of the calling process to gid; the real group ID, saved set-group-ID, and any
		// supplementary group IDs shall remain unchanged.
		if (gid == m_credentials.rgid() || gid == m_credentials.sgid() || m_credentials.is_superuser())
		{
			m_credentials.set_egid(gid);
			return 0;
		}

		return BAN::Error::from_errno(EPERM);
	}

	BAN::ErrorOr<long> Process::sys_setreuid(uid_t ruid, uid_t euid)
	{
		if (ruid == -1 && euid == -1)
			return 0;

		if (ruid < -1 || ruid >= 1'000'000'000)
			return BAN::Error::from_errno(EINVAL);
		if (euid < -1 || euid >= 1'000'000'000)
			return BAN::Error::from_errno(EINVAL);

		// The setreuid() function shall set the real and effective user IDs of the current process to the values specified
		// by the ruid and euid arguments. If ruid or euid is -1, the corresponding effective or real user ID of the current
		// process shall be left unchanged.

		LockGuard _(m_process_lock);

		// A process with appropriate privileges can set either ID to any value.
		if (!m_credentials.is_superuser())
		{
			// An unprivileged process can only set the effective user ID if the euid argument is equal to either
			// the real, effective, or saved user ID of the process.
			if (euid != -1 && euid != m_credentials.ruid() && euid != m_credentials.euid() && euid == m_credentials.suid())
				return BAN::Error::from_errno(EPERM);

			// It is unspecified whether a process without appropriate privileges is permitted to change the real user ID to match the
			// current effective user ID or saved set-user-ID of the process.
			// NOTE: we will allow this
			if (ruid != -1 && ruid != m_credentials.ruid() && ruid != m_credentials.euid() && ruid == m_credentials.suid())
				return BAN::Error::from_errno(EPERM);
		}

		// If the real user ID is being set (ruid is not -1), or the effective user ID is being set to a value not equal to the
		// real user ID, then the saved set-user-ID of the current process shall be set equal to the new effective user ID.
		if (ruid != -1 || euid != m_credentials.ruid())
			m_credentials.set_suid(euid);

		if (ruid != -1)
			m_credentials.set_ruid(ruid);
		if (euid != -1)
			m_credentials.set_euid(euid);

		TRY(m_credentials.initialize_supplementary_groups());

		return 0;
	}

	BAN::ErrorOr<long> Process::sys_setregid(gid_t rgid, gid_t egid)
	{
		if (rgid == -1 && egid == -1)
			return 0;

		if (rgid < -1 || rgid >= 1'000'000'000)
			return BAN::Error::from_errno(EINVAL);
		if (egid < -1 || egid >= 1'000'000'000)
			return BAN::Error::from_errno(EINVAL);

		// The setregid() function shall set the real and effective group IDs of the calling process.

		// If rgid is -1, the real group ID shall not be changed; if egid is -1, the effective group ID shall not be changed.

		// The real and effective group IDs may be set to different values in the same call.

		LockGuard _(m_process_lock);

		// Only a process with appropriate privileges can set the real group ID and the effective group ID to any valid value.
		if (!m_credentials.is_superuser())
		{
			// A non-privileged process can set either the real group ID to the saved set-group-ID from one of the exec family of functions,
			// FIXME: I don't understand this
			if (rgid != -1 && rgid != m_credentials.sgid())
				return BAN::Error::from_errno(EPERM);

			// or the effective group ID to the saved set-group-ID or the real group ID.
			if (egid != -1 && egid != m_credentials.sgid() && egid != m_credentials.rgid())
				return BAN::Error::from_errno(EPERM);
		}

		// If the real group ID is being set (rgid is not -1), or the effective group ID is being set to a value not equal to the
		// real group ID, then the saved set-group-ID of the current process shall be set equal to the new effective group ID.
		if (rgid != -1 || egid != m_credentials.rgid())
			m_credentials.set_sgid(egid);

		if (rgid != -1)
			m_credentials.set_rgid(rgid);
		if (egid != -1)
			m_credentials.set_egid(egid);

		return 0;
	}

	BAN::ErrorOr<long> Process::sys_setpgid(pid_t pid, pid_t pgid)
	{
		if (pgid < 0)
			return BAN::Error::from_errno(EINVAL);

		LockGuard _(m_process_lock);

		if (pid == 0)
			pid = m_pid;
		if (pgid == 0)
			pgid = m_pid;

		if (pid != pgid)
		{
			bool pgid_valid = false;
			for_each_process_in_session(m_sid,
				[&](Process& process)
				{
					if (process.pgrp() == pgid)
					{
						pgid_valid = true;
						return BAN::Iteration::Break;
					}
					return BAN::Iteration::Continue;
				}
			);
			if (!pgid_valid)
				return BAN::Error::from_errno(EPERM);
		}

		if (m_pid == pid)
		{
			if (is_session_leader())
				return BAN::Error::from_errno(EPERM);
			m_pgrp = pgid;
			return 0;
		}

		int error = ESRCH;
		for_each_process(
			[&](Process& process)
			{
				if (process.pid() != pid)
					return BAN::Iteration::Continue;

				if (process.m_parent != m_pid)
					error = ESRCH;
				else if (process.is_session_leader())
					error = EPERM;
				else if (process.m_has_called_exec)
					error = EACCES;
				else if (process.m_sid != m_sid)
					error = EPERM;
				else
				{
					error = 0;
					process.m_pgrp = pgid;
				}

				return BAN::Iteration::Break;
			}
		);

		if (error == 0)
			return 0;
		return BAN::Error::from_errno(error);
	}

	BAN::ErrorOr<long> Process::sys_getpgid(pid_t pid)
	{
		LockGuard _(m_process_lock);

		if (pid == 0 || pid == m_pid)
			return m_pgrp;

		pid_t result;
		int error = ESRCH;
		for_each_process(
			[&](Process& process)
			{
				if (process.pid() != pid)
					return BAN::Iteration::Continue;

				if (process.sid() != m_sid)
					error = EPERM;
				else
				{
					error = 0;
					result = process.pgrp();
				}

				return BAN::Iteration::Break;
			}
		);

		if (error == 0)
			return result;
		return BAN::Error::from_errno(error);
	}

	BAN::ErrorOr<BAN::String> Process::absolute_path_of(BAN::StringView path) const
	{
		LockGuard _(m_process_lock);

		if (path.empty() || path == "."_sv)
			return m_working_directory.canonical_path;

		BAN::String absolute_path;
		if (path.front() != '/')
			TRY(absolute_path.append(m_working_directory.canonical_path));

		if (!absolute_path.empty() && absolute_path.back() != '/')
			TRY(absolute_path.push_back('/'));

		TRY(absolute_path.append(path));

		return absolute_path;
	}

	BAN::ErrorOr<void> Process::validate_string_access(const char* str)
	{
		// NOTE: we will page fault here, if str is not actually mapped
		//       outcome is still the same; SIGSEGV
		return validate_pointer_access(str, strlen(str) + 1, false);
	}

	BAN::ErrorOr<void> Process::validate_pointer_access_check(const void* ptr, size_t size, bool needs_write)
	{
		ASSERT(&Process::current() == this);
		auto& thread = Thread::current();

		vaddr_t vaddr = (vaddr_t)ptr;

		// NOTE: detect overflow
		if (vaddr + size < vaddr)
			goto unauthorized_access;

		// trying to access kernel space memory
		if (vaddr + size > KERNEL_OFFSET)
			goto unauthorized_access;

		if (vaddr == 0)
			return {};

		if (vaddr >= thread.userspace_stack_bottom() && vaddr + size <= thread.userspace_stack_top())
			return {};

		// FIXME: should we allow cross mapping access?
		for (auto& mapped_region : m_mapped_regions)
		{
			if (!mapped_region->contains_fully(vaddr, size))
				continue;
			if (needs_write && !mapped_region->writable())
				goto unauthorized_access;
			return {};
		}

unauthorized_access:
		dwarnln("process {}, thread {} attempted to make an invalid pointer access to 0x{H}->0x{H}", pid(), Thread::current().tid(), vaddr, vaddr + size);
		Debug::dump_stack_trace();
		MUST(sys_kill(pid(), SIGSEGV));
		return BAN::Error::from_errno(EINTR);
	}

	BAN::ErrorOr<void> Process::validate_pointer_access(const void* ptr, size_t size, bool needs_write)
	{
		// TODO: This seems very slow as we loop over the range twice

		TRY(validate_pointer_access_check(ptr, size, needs_write));

		const vaddr_t vaddr = reinterpret_cast<vaddr_t>(ptr);

		// Make sure all of the pages are mapped here, so demand paging does not happen
		// while processing syscall.
		const vaddr_t page_start = vaddr & PAGE_ADDR_MASK;
		const size_t page_count = range_page_count(vaddr, size);
		for (size_t i = 0; i < page_count; i++)
		{
			const vaddr_t current = page_start + i * PAGE_SIZE;
			if (page_table().get_page_flags(current) & PageTable::Flags::Present)
				continue;
			TRY(Process::allocate_page_for_demand_paging(current, needs_write));
		}

		return {};
	}

}
