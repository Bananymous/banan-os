#include <BAN/StringView.h>
#include <kernel/CriticalScope.h>
#include <kernel/FS/Pipe.h>
#include <kernel/FS/VirtualFileSystem.h>
#include <kernel/LockGuard.h>
#include <kernel/Memory/Heap.h>
#include <kernel/Memory/PageTableScope.h>
#include <kernel/Process.h>
#include <kernel/Scheduler.h>
#include <LibELF/ELF.h>
#include <LibELF/Values.h>

#include <fcntl.h>
#include <stdio.h>

namespace Kernel
{

	static BAN::Vector<Process*> s_processes;
	static SpinLock s_process_lock;

	Process* Process::create_process(const Credentials& credentials)
	{
		static pid_t s_next_pid = 1;
		auto* process = new Process(credentials, s_next_pid++);
		ASSERT(process);
		return process;
	}

	void Process::register_process(Process* process)
	{
		s_process_lock.lock();
		MUST(s_processes.push_back(process));
		s_process_lock.unlock();
		for (auto* thread : process->m_threads)
			MUST(Scheduler::get().add_thread(thread));
	}

	Process* Process::create_kernel(entry_t entry, void* data)
	{
		auto* process = create_process({ 0, 0, 0, 0 });
		MUST(process->m_working_directory.push_back('/'));
		auto* thread = MUST(Thread::create_kernel(entry, data, process));
		process->add_thread(thread);
		register_process(process);
		return process;
	}

	BAN::ErrorOr<Process*> Process::create_userspace(const Credentials& credentials, BAN::StringView path)
	{
		auto elf = TRY(load_elf_for_exec(credentials, path, "/"sv, {}));

		auto* process = create_process(credentials);
		MUST(process->m_working_directory.push_back('/'));
		process->m_page_table = BAN::UniqPtr<PageTable>::adopt(MUST(PageTable::create_userspace()));;

		process->load_elf_to_memory(*elf);

		process->m_userspace_info.entry = elf->file_header_native().e_entry;

		// NOTE: we clear the elf since we don't need the memory anymore
		elf.clear();

		char** argv = nullptr;
		char** envp = nullptr;
		{
			PageTableScope _(process->page_table());

			argv = (char**)MUST(process->sys_alloc(sizeof(char**) * 2));
			argv[0] = (char*)MUST(process->sys_alloc(path.size() + 1));
			memcpy(argv[0], path.data(), path.size());
			argv[0][path.size()] = '\0';
			argv[1] = nullptr;

			BAN::StringView env1 = "PATH=/bin:/usr/bin"sv;
			envp = (char**)MUST(process->sys_alloc(sizeof(char**) * 2));
			envp[0] = (char*)MUST(process->sys_alloc(env1.size() + 1));
			memcpy(envp[0], env1.data(), env1.size());
			envp[0][env1.size()] = '\0';
			envp[1] = nullptr;
		}

		process->m_userspace_info.argc = 1;
		process->m_userspace_info.argv = argv;
		process->m_userspace_info.envp = envp;

		auto* thread = MUST(Thread::create_userspace(process));
		process->add_thread(thread);
		register_process(process);
		return process;
	}

	Process::Process(const Credentials& credentials, pid_t pid)
		: m_credentials(credentials)
		, m_pid(pid)
		, m_tty(TTY::current())
	{ }

	Process::~Process()
	{
		ASSERT(m_threads.empty());
		ASSERT(m_fixed_width_allocators.empty());
		ASSERT(!m_general_allocator);
		ASSERT(m_mapped_ranges.empty());
		ASSERT(&PageTable::current() != m_page_table.ptr());
	}

	void Process::add_thread(Thread* thread)
	{
		LockGuard _(m_lock);
		MUST(m_threads.push_back(thread));
	}

	void Process::on_thread_exit(Thread& thread)
	{
		LockGuard _(m_lock);
		for (size_t i = 0; i < m_threads.size(); i++)
			if (m_threads[i] == &thread)
				m_threads.remove(i);
		if (m_threads.empty())
			exit(0);
	}

	void Process::exit(int status)
	{
		m_lock.lock();
		m_exit_status.exit_code = status;
		m_exit_status.exited = true;
		while (m_exit_status.waiting > 0)
		{
			m_exit_status.semaphore.unblock();
			m_lock.unlock();
			Scheduler::get().reschedule();
			m_lock.lock();
		}

		m_threads.clear();

		for (int fd = 0; fd < (int)m_open_files.size(); fd++)
		{
			if (validate_fd(fd).is_error())
				continue;
			(void)sys_close(fd);
		}

		// NOTE: We must unmap ranges while the page table is still alive
		for (auto* range : m_mapped_ranges)
			delete range;
		m_mapped_ranges.clear();

		// NOTE: We must clear allocators while the page table is still alive
		m_fixed_width_allocators.clear();
		m_general_allocator.clear();

		s_process_lock.lock();
		for (size_t i = 0; i < s_processes.size(); i++)
			if (s_processes[i] == this)
				s_processes.remove(i);
		s_process_lock.unlock();

		// FIXME: we can't assume this is the current process
		ASSERT(&Process::current() == this);
		Scheduler::get().set_current_process_done();
	}

	BAN::ErrorOr<long> Process::sys_exit(int status)
	{
		exit(status);
		ASSERT_NOT_REACHED();
	}

	BAN::ErrorOr<long> Process::sys_gettermios(::termios* termios)
	{
		LockGuard _(m_lock);
		if (m_tty == nullptr)
			return BAN::Error::from_errno(ENOTTY);
		
		Kernel::termios ktermios = m_tty->get_termios();
		termios->c_lflag = 0;
		if (ktermios.canonical)
			termios->c_lflag |= ICANON;
		if (ktermios.echo)
			termios->c_lflag |= ECHO;

		return 0;
	}

	BAN::ErrorOr<long> Process::sys_settermios(const ::termios* termios)
	{
		LockGuard _(m_lock);
		if (m_tty == nullptr)
			return BAN::Error::from_errno(ENOTTY);
		
		Kernel::termios ktermios;
		ktermios.echo = termios->c_lflag & ECHO;
		ktermios.canonical = termios->c_lflag & ICANON;
		
		m_tty->set_termios(ktermios);
		return 0;
	}

	BAN::ErrorOr<BAN::UniqPtr<LibELF::ELF>> Process::load_elf_for_exec(const Credentials& credentials, BAN::StringView file_path, const BAN::String& cwd, const BAN::Vector<BAN::StringView>& path_env)
	{
		if (file_path.empty())
			return BAN::Error::from_errno(ENOENT);

		BAN::String absolute_path;

		if (file_path.front() == '/')
		{
			// We have an absolute path
			TRY(absolute_path.append(file_path));
		}
		else if (file_path.front() == '.' || file_path.contains('/'))
		{
			// We have a relative path
			TRY(absolute_path.append(cwd));
			TRY(absolute_path.push_back('/'));
			TRY(absolute_path.append(file_path));
		}
		else
		{
			// We have neither relative or absolute path,
			// search from PATH environment
			for (auto path_part : path_env)
			{
				if (path_part.empty())
					continue;

				if (path_part.front() != '/')
				{
					TRY(absolute_path.append(cwd));
					TRY(absolute_path.push_back('/'));
				}
				TRY(absolute_path.append(path_part));
				TRY(absolute_path.push_back('/'));
				TRY(absolute_path.append(file_path));

				if (!VirtualFileSystem::get().file_from_absolute_path(credentials, absolute_path, O_EXEC).is_error())
					break;

				absolute_path.clear();
			}

			if (absolute_path.empty())
				return BAN::Error::from_errno(ENOENT);
		}

		auto file = TRY(VirtualFileSystem::get().file_from_absolute_path(credentials, absolute_path, O_EXEC));

		auto elf_or_error = LibELF::ELF::load_from_file(file.inode);
		if (elf_or_error.is_error())
		{
			if (elf_or_error.error().get_error_code() == EINVAL)
				return BAN::Error::from_errno(ENOEXEC);
			return elf_or_error.error();
		}
		
		auto elf = elf_or_error.release_value();
		if (!elf->is_native())
		{
			derrorln("ELF has invalid architecture");
			return BAN::Error::from_errno(EINVAL);
		}

		if (elf->file_header_native().e_type != LibELF::ET_EXEC)
		{
			derrorln("Not an executable");
			return BAN::Error::from_errno(ENOEXEC);
		}

		return BAN::move(elf);
	}

	BAN::ErrorOr<long> Process::sys_fork(uintptr_t rsp, uintptr_t rip)
	{
		Process* forked = create_process(m_credentials);

		forked->m_page_table = BAN::UniqPtr<PageTable>::adopt(MUST(PageTable::create_userspace()));

		LockGuard _(m_lock);
		forked->m_tty = m_tty;
		forked->m_working_directory = m_working_directory;

		forked->m_open_files = m_open_files;
		for (int fd = 0; fd < (int)m_open_files.size(); fd++)
		{
			if (validate_fd(fd).is_error())
				continue;
			auto& openfd = open_file_description(fd);
			if (openfd.flags & O_WRONLY && openfd.inode->is_pipe())
				((Pipe*)openfd.inode.ptr())->clone_writing();
		}

		forked->m_userspace_info = m_userspace_info;

		for (auto* mapped_range : m_mapped_ranges)
			MUST(forked->m_mapped_ranges.push_back(mapped_range->clone(forked->page_table())));

		for (auto& allocator : m_fixed_width_allocators)
			if (allocator->allocations() > 0)
				MUST(forked->m_fixed_width_allocators.push_back(MUST(allocator->clone(forked->page_table()))));

		if (m_general_allocator)
			forked->m_general_allocator = MUST(m_general_allocator->clone(forked->page_table()));

		ASSERT(this == &Process::current());
		Thread* thread = MUST(Thread::current().clone(forked, rsp, rip));
		forked->add_thread(thread);
		
		register_process(forked);

		return forked->pid();
	}

	BAN::ErrorOr<long> Process::sys_exec(BAN::StringView path, const char* const* argv, const char* const* envp)
	{
		BAN::Vector<BAN::String> str_argv;
		for (int i = 0; argv && argv[i]; i++)
			TRY(str_argv.emplace_back(argv[i]));

		BAN::Vector<BAN::StringView> path_env;
		BAN::Vector<BAN::String> str_envp;
		for (int i = 0; envp && envp[i]; i++)
		{
			TRY(str_envp.emplace_back(envp[i]));
			if (strncmp(envp[i], "PATH=", 5) == 0)
				path_env = TRY(BAN::StringView(envp[i]).substring(5).split(':'));
		}

		BAN::String working_directory;

		{
			LockGuard _(m_lock);
			TRY(working_directory.append(m_working_directory));
		}

		auto elf = TRY(load_elf_for_exec(m_credentials, path, working_directory, path_env));

		LockGuard lock_guard(m_lock);

		for (int fd = 0; fd < (int)m_open_files.size(); fd++)
		{
			if (validate_fd(fd).is_error())
				continue;
			auto& file = open_file_description(fd);
			if (file.flags & O_CLOEXEC)
				MUST(sys_close(fd));
		}

		m_fixed_width_allocators.clear();
		m_general_allocator.clear();

		for (auto* range : m_mapped_ranges)
			delete range;
		m_mapped_ranges.clear();

		load_elf_to_memory(*elf);

		m_userspace_info.entry = elf->file_header_native().e_entry;

		// NOTE: we clear the elf since we don't need the memory anymore
		elf.clear();

		ASSERT(m_threads.size() == 1);
		ASSERT(&Process::current() == this);

		{
			LockGuard _(page_table());

			m_userspace_info.argv = (char**)MUST(sys_alloc(sizeof(char**) * (str_argv.size() + 1)));
			for (size_t i = 0; i < str_argv.size(); i++)
			{
				m_userspace_info.argv[i] = (char*)MUST(sys_alloc(str_argv[i].size() + 1));
				memcpy(m_userspace_info.argv[i], str_argv[i].data(), str_argv[i].size());
				m_userspace_info.argv[i][str_argv[i].size()] = '\0';
			}
			m_userspace_info.argv[str_argv.size()] = nullptr;

			m_userspace_info.envp = (char**)MUST(sys_alloc(sizeof(char**) * (str_envp.size() + 1)));
			for (size_t i = 0; i < str_envp.size(); i++)
			{
				m_userspace_info.envp[i] = (char*)MUST(sys_alloc(str_envp[i].size() + 1));
				memcpy(m_userspace_info.envp[i], str_envp[i].data(), str_envp[i].size());
				m_userspace_info.envp[i][str_envp[i].size()] = '\0';
			}
			m_userspace_info.envp[str_envp.size()] = nullptr;
		}

		m_userspace_info.argc = str_argv.size();

		// NOTE: These must be manually cleared since this function won't return after this point
		str_argv.clear();
		str_envp.clear();

		CriticalScope _;
		lock_guard.~LockGuard();
		m_threads.front()->setup_exec();
		Scheduler::get().execute_current_thread();
		ASSERT_NOT_REACHED();
	}

	int Process::block_until_exit()
	{
		ASSERT(s_process_lock.is_locked());
		ASSERT(this != &Process::current());

		s_process_lock.unlock();

		m_lock.lock();
		m_exit_status.waiting++;
		while (!m_exit_status.exited)
		{
			m_lock.unlock();
			m_exit_status.semaphore.block();
			m_lock.lock();
		}

		int ret = m_exit_status.exit_code;
		m_exit_status.waiting--;
		m_lock.unlock();

		s_process_lock.lock();

		return ret;
	}

	BAN::ErrorOr<long> Process::sys_wait(pid_t pid, int* stat_loc, int options)
	{
		Process* target = nullptr;

		// FIXME: support options
		if (options)
			return BAN::Error::from_errno(EINVAL);

		LockGuard _(s_process_lock);
		for (auto* process : s_processes)
			if (process->pid() == pid)
				target = process;

		if (target == nullptr)
			return BAN::Error::from_errno(ECHILD);

		pid_t ret = target->pid();
		*stat_loc = target->block_until_exit();

		return ret;
	}

	BAN::ErrorOr<long> Process::sys_sleep(int seconds)
	{
		PIT::sleep(seconds * 1000);
		return 0;
	}

	BAN::ErrorOr<long> Process::sys_setenvp(char** envp)
	{
		LockGuard _(m_lock);
		m_userspace_info.envp = envp;
		return 0;
	}

	void Process::load_elf_to_memory(LibELF::ELF& elf)
	{
		ASSERT(elf.is_native());

		auto& elf_file_header = elf.file_header_native();
		for (size_t i = 0; i < elf_file_header.e_phnum; i++)
		{
			auto& elf_program_header = elf.program_header_native(i);

			switch (elf_program_header.p_type)
			{
			case LibELF::PT_NULL:
				break;
			case LibELF::PT_LOAD:
			{
				uint8_t flags = PageTable::Flags::UserSupervisor | PageTable::Flags::Present;
				if (elf_program_header.p_flags & LibELF::PF_W)
					flags |= PageTable::Flags::ReadWrite;

				size_t page_start = elf_program_header.p_vaddr / PAGE_SIZE;
				size_t page_end = BAN::Math::div_round_up<size_t>(elf_program_header.p_vaddr + elf_program_header.p_memsz, PAGE_SIZE);
				size_t page_count = page_end - page_start;

				page_table().lock();

				if (!page_table().is_range_free(page_start * PAGE_SIZE, page_count * PAGE_SIZE))
				{
					page_table().debug_dump();
					Kernel::panic("vaddr {8H}-{8H} not free {8H}-{8H}",
						page_start * PAGE_SIZE,
						page_start * PAGE_SIZE + page_count * PAGE_SIZE
					);
				}

				{
					LockGuard _(m_lock);
					MUST(m_mapped_ranges.push_back(VirtualRange::create(page_table(), page_start * PAGE_SIZE, page_count * PAGE_SIZE, flags)));
					m_mapped_ranges.back()->set_zero();
					m_mapped_ranges.back()->copy_from(elf_program_header.p_vaddr % PAGE_SIZE, elf.data() + elf_program_header.p_offset, elf_program_header.p_filesz);
				}

				page_table().unlock();

				break;
			}
			default:
				ASSERT_NOT_REACHED();
			}
		}
	}

	BAN::ErrorOr<long> Process::sys_open(BAN::StringView path, int flags)
	{
		if (flags & ~(O_RDONLY | O_WRONLY | O_NOFOLLOW | O_SEARCH | O_CLOEXEC))
			return BAN::Error::from_errno(ENOTSUP);

		BAN::String absolute_path = TRY(absolute_path_of(path));

		auto file = TRY(VirtualFileSystem::get().file_from_absolute_path(m_credentials, absolute_path, flags));

		LockGuard _(m_lock);
		int fd = TRY(get_free_fd());
		auto& open_file_description = m_open_files[fd];
		open_file_description.inode = file.inode;
		open_file_description.path = BAN::move(file.canonical_path);
		open_file_description.offset = 0;
		open_file_description.flags = flags;

		return fd;
	}

	BAN::ErrorOr<long> Process::sys_openat(int fd, BAN::StringView path, int flags)
	{
		BAN::String absolute_path;
		
		{
			LockGuard _(m_lock);
			TRY(validate_fd(fd));
			TRY(absolute_path.append(open_file_description(fd).path));
		}
		
		TRY(absolute_path.push_back('/'));
		TRY(absolute_path.append(path));

		return sys_open(absolute_path, flags);
	}

	BAN::ErrorOr<long> Process::sys_close(int fd)
	{
		LockGuard _(m_lock);
		TRY(validate_fd(fd));
		auto& open_file_description = this->open_file_description(fd);
		if (open_file_description.flags & O_WRONLY && open_file_description.inode->is_pipe())
			((Pipe*)open_file_description.inode.ptr())->close_writing();
		open_file_description.inode = nullptr;
		return 0;
	}

	BAN::ErrorOr<long> Process::sys_read(int fd, void* buffer, size_t count)
	{
		OpenFileDescription open_fd_copy;

		{
			LockGuard _(m_lock);
			TRY(validate_fd(fd));
			open_fd_copy = open_file_description(fd);
		}

		if (!(open_fd_copy.flags & O_RDONLY))
			return BAN::Error::from_errno(EBADF);

		size_t nread = TRY(open_fd_copy.inode->read(open_fd_copy.offset, buffer, count));

		{
			LockGuard _(m_lock);
			MUST(validate_fd(fd));
			open_file_description(fd).offset += nread;
		}

		return nread;
	}

	BAN::ErrorOr<long> Process::sys_write(int fd, const void* buffer, size_t count)
	{
		OpenFileDescription open_fd_copy;

		{
			LockGuard _(m_lock);
			TRY(validate_fd(fd));
			open_fd_copy = open_file_description(fd);
		}

		if (!(open_fd_copy.flags & O_WRONLY))
			return BAN::Error::from_errno(EBADF);

		size_t nwrite = TRY(open_fd_copy.inode->write(open_fd_copy.offset, buffer, count));

		{
			LockGuard _(m_lock);
			MUST(validate_fd(fd));
			open_file_description(fd).offset += nwrite;
		}

		return nwrite;
	}

	BAN::ErrorOr<long> Process::sys_pipe(int fildes[2])
	{
		LockGuard _(m_lock);

		auto pipe = TRY(Pipe::create(m_credentials));

		TRY(get_free_fd_pair(fildes));

		auto& openfd_read = m_open_files[fildes[0]];
		openfd_read.inode = pipe;
		openfd_read.flags = O_RDONLY;
		openfd_read.offset = 0;
		openfd_read.path.clear();

		auto& openfd_write = m_open_files[fildes[1]];
		openfd_write.inode = pipe;
		openfd_write.flags = O_WRONLY;
		openfd_write.offset = 0;
		openfd_write.path.clear();

		return 0;
	}

	BAN::ErrorOr<long> Process::sys_dup2(int fildes, int fildes2)
	{
		// FIXME: m_open_files should be BAN::Array<BAN::RefPtr<OpenFileDescription>, OPEN_MAX> for accurate dup2

		if (fildes2 < 0 || fildes2 >= 100)
			return BAN::Error::from_errno(EBADF);

		LockGuard _(m_lock);

		TRY(validate_fd(fildes));
		if (fildes == fildes2)
			return fildes;

		if (m_open_files.size() <= (size_t)fildes2)
			TRY(m_open_files.resize(fildes2 + 1));
		
		if (!validate_fd(fildes2).is_error())
			TRY(sys_close(fildes2));
		
		m_open_files[fildes2] = m_open_files[fildes];
		m_open_files[fildes2].flags &= ~O_CLOEXEC;

		if (m_open_files[fildes].flags & O_WRONLY && m_open_files[fildes].inode->is_pipe())
			((Pipe*)m_open_files[fildes].inode.ptr())->clone_writing();

		return fildes;
	}

	BAN::ErrorOr<long> Process::sys_seek(int fd, off_t offset, int whence)
	{
		LockGuard _(m_lock);
		TRY(validate_fd(fd));

		auto& open_fd = open_file_description(fd);

		off_t new_offset = 0;

		switch (whence)
		{
			case SEEK_CUR:
				new_offset = open_fd.offset + offset;
				break;
			case SEEK_END:
				new_offset = open_fd.inode->size() - offset;
				break;
			case SEEK_SET:
				new_offset = offset;
				break;
			default:
				return BAN::Error::from_errno(EINVAL);
		}

		if (new_offset < 0)
			return BAN::Error::from_errno(EINVAL);
		open_fd.offset = new_offset;

		return 0;
	}

	BAN::ErrorOr<long> Process::sys_tell(int fd)
	{
		LockGuard _(m_lock);
		TRY(validate_fd(fd));
		return open_file_description(fd).offset;
	}

	BAN::ErrorOr<long> Process::sys_creat(BAN::StringView path, mode_t mode)
	{
		auto absolute_path = TRY(absolute_path_of(path));

		size_t index;
		for (index = absolute_path.size(); index > 0; index--)
			if (absolute_path[index - 1] == '/')
				break;

		auto directory = absolute_path.sv().substring(0, index);
		auto file_name = absolute_path.sv().substring(index);

		auto parent_file = TRY(VirtualFileSystem::get().file_from_absolute_path(m_credentials, directory, O_WRONLY));
		TRY(parent_file.inode->create_file(file_name, mode));

		return 0;
	}

	BAN::ErrorOr<void> Process::mount(BAN::StringView source, BAN::StringView target)
	{
		BAN::String absolute_source, absolute_target;
		{
			LockGuard _(m_lock);
			absolute_source = TRY(absolute_path_of(source));
			absolute_target = TRY(absolute_path_of(target));
		}
		TRY(VirtualFileSystem::get().mount(m_credentials, absolute_source, absolute_target));
		return {};
	}

	BAN::ErrorOr<long> Process::sys_fstat(int fd, struct stat* out)
	{
		OpenFileDescription open_fd_copy;

		{
			LockGuard _(m_lock);
			TRY(validate_fd(fd));
			open_fd_copy = open_file_description(fd);
		}

		out->st_dev		= open_fd_copy.inode->dev();
		out->st_ino		= open_fd_copy.inode->ino();
		out->st_mode	= open_fd_copy.inode->mode().mode;
		out->st_nlink	= open_fd_copy.inode->nlink();
		out->st_uid		= open_fd_copy.inode->uid();
		out->st_gid		= open_fd_copy.inode->gid();
		out->st_rdev	= open_fd_copy.inode->rdev();
		out->st_size	= open_fd_copy.inode->size();
		out->st_atim	= open_fd_copy.inode->atime();
		out->st_mtim	= open_fd_copy.inode->mtime();
		out->st_ctim	= open_fd_copy.inode->ctime();
		out->st_blksize	= open_fd_copy.inode->blksize();
		out->st_blocks	= open_fd_copy.inode->blocks();

		return 0;
	}

	BAN::ErrorOr<long> Process::sys_stat(BAN::StringView path, struct stat* out, int flags)
	{
		int fd = TRY(sys_open(path, flags));
		auto ret = sys_fstat(fd, out);
		MUST(sys_close(fd));
		return ret;
	}

	BAN::ErrorOr<long> Process::sys_read_dir_entries(int fd, DirectoryEntryList* list, size_t list_size)
	{
		OpenFileDescription open_fd_copy;

		{
			LockGuard _(m_lock);
			TRY(validate_fd(fd));
			open_fd_copy = open_file_description(fd);
		}

		TRY(open_fd_copy.inode->read_next_directory_entries(open_fd_copy.offset, list, list_size));

		{
			LockGuard _(m_lock);
			MUST(validate_fd(fd));
			open_file_description(fd).offset = open_fd_copy.offset + 1;
		}

		return 0;
	}

	BAN::ErrorOr<long> Process::sys_setpwd(const char* path)
	{
		BAN::String absolute_path;

		{
			LockGuard _(m_lock);
			absolute_path = TRY(absolute_path_of(path));
		}

		auto file = TRY(VirtualFileSystem::get().file_from_absolute_path(m_credentials, absolute_path, O_SEARCH));
		if (!file.inode->mode().ifdir())
			return BAN::Error::from_errno(ENOTDIR);

		LockGuard _(m_lock);
		m_working_directory = BAN::move(file.canonical_path);

		return 0;
	}

	BAN::ErrorOr<long> Process::sys_getpwd(char* buffer, size_t size)
	{
		LockGuard _(m_lock);

		if (size < m_working_directory.size() + 1)
			return BAN::Error::from_errno(ERANGE);
		
		memcpy(buffer, m_working_directory.data(), m_working_directory.size());
		buffer[m_working_directory.size()] = '\0';

		return (long)buffer;
	}

	static constexpr size_t allocator_size_for_allocation(size_t value)
	{
		if (value <= 256) {
			if (value <= 64)
				return 64;
			else
				return 256;
		} else {
			if (value <= 1024)
				return 1024;
			else
				return 4096;
		}
	}

	BAN::ErrorOr<long> Process::sys_alloc(size_t bytes)
	{
		vaddr_t address = 0;

		if (bytes <= PAGE_SIZE)
		{
			// Do fixed width allocation
			size_t allocation_size = allocator_size_for_allocation(bytes);
			ASSERT(bytes <= allocation_size);
			ASSERT(allocation_size <= PAGE_SIZE);

			LockGuard _(m_lock);

			bool needs_new_allocator { true };

			for (auto& allocator : m_fixed_width_allocators)
			{
				if (allocator->allocation_size() == allocation_size && allocator->allocations() < allocator->max_allocations())
				{
					address = allocator->allocate();
					needs_new_allocator = false;
					break;
				}
			}

			if (needs_new_allocator)
			{
				auto allocator = TRY(FixedWidthAllocator::create(page_table(), allocation_size));
				TRY(m_fixed_width_allocators.push_back(BAN::move(allocator)));
				address = m_fixed_width_allocators.back()->allocate();
			}
		}
		else
		{
			LockGuard _(m_lock);

			if (!m_general_allocator)
				m_general_allocator = TRY(GeneralAllocator::create(page_table(), 0x400000));

			address = m_general_allocator->allocate(bytes);
		}

		if (address == 0)
			return BAN::Error::from_errno(ENOMEM);
		return address;
	}

	BAN::ErrorOr<long> Process::sys_free(void* ptr)
	{
		LockGuard _(m_lock);

		for (size_t i = 0; i < m_fixed_width_allocators.size(); i++)
		{
			auto& allocator = m_fixed_width_allocators[i];
			if (allocator->deallocate((vaddr_t)ptr))
			{
				// TODO: This might be too much. Maybe we should only
				//       remove allocators when we have low memory... ?
				if (allocator->allocations() == 0)
					m_fixed_width_allocators.remove(i);
				return 0;
			}
		}

		if (m_general_allocator && m_general_allocator->deallocate((vaddr_t)ptr))
			return 0;

		dwarnln("free called on pointer that was not allocated");
		return BAN::Error::from_errno(EINVAL);
	}

	BAN::ErrorOr<long> Process::sys_termid(char* buffer) const
	{
		LockGuard _(m_lock);
		if (m_tty == nullptr)
			buffer[0] = '\0';
		strcpy(buffer, "/dev/");
		strcpy(buffer + 5, m_tty->name().data());
		return 0;
	}


	BAN::ErrorOr<long> Process::sys_clock_gettime(clockid_t clock_id, timespec* tp) const
	{
		switch (clock_id)
		{
			case CLOCK_MONOTONIC:
			{
				uint64_t time_ms = PIT::ms_since_boot();
				tp->tv_sec  =  time_ms / 1000;
				tp->tv_nsec = (time_ms % 1000) * 1000000;
				break;
			}
			default:
				return BAN::Error::from_errno(ENOTSUP);
		}
		return 0;
	}

	BAN::ErrorOr<long> Process::sys_setuid(uid_t uid)
	{
		if (uid < 0 || uid >= 1'000'000'000)
			return BAN::Error::from_errno(EINVAL);

		LockGuard _(m_lock);

		// If the process has appropriate privileges, setuid() shall set the real user ID, effective user ID, and the saved
		// set-user-ID of the calling process to uid.
		if (m_credentials.is_superuser())
		{
			m_credentials.set_euid(uid);
			m_credentials.set_ruid(uid);
			m_credentials.set_suid(uid);
			return 0;
		}

		// If the process does not have appropriate privileges, but uid is equal to the real user ID or the saved set-user-ID,
		// setuid() shall set the effective user ID to uid; the real user ID and saved set-user-ID shall remain unchanged.
		if (uid == m_credentials.ruid() || uid == m_credentials.suid())
		{
			m_credentials.set_euid(uid);
			return 0;
		}

		return BAN::Error::from_errno(EPERM);
	}

	BAN::ErrorOr<long> Process::sys_setgid(gid_t gid)
	{
		if (gid < 0 || gid >= 1'000'000'000)
			return BAN::Error::from_errno(EINVAL);

		LockGuard _(m_lock);

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

	BAN::ErrorOr<long> Process::sys_seteuid(uid_t uid)
	{
		if (uid < 0 || uid >= 1'000'000'000)
			return BAN::Error::from_errno(EINVAL);

		LockGuard _(m_lock);

		// If uid is equal to the real user ID or the saved set-user-ID, or if the process has appropriate privileges, seteuid()
		// shall set the effective user ID of the calling process to uid; the real user ID and saved set-user-ID shall remain unchanged.
		if (uid == m_credentials.ruid() || uid == m_credentials.suid() || m_credentials.is_superuser())
		{
			m_credentials.set_euid(uid);
			return 0;
		}

		return BAN::Error::from_errno(EPERM);
	}

	BAN::ErrorOr<long> Process::sys_setegid(gid_t gid)
	{
		if (gid < 0 || gid >= 1'000'000'000)
			return BAN::Error::from_errno(EINVAL);

		LockGuard _(m_lock);

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

		LockGuard _(m_lock);

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

		LockGuard _(m_lock);

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

	BAN::ErrorOr<BAN::String> Process::absolute_path_of(BAN::StringView path) const
	{
		if (path.empty() || path == "."sv)
		{
			LockGuard _(m_lock);
			return m_working_directory;
		}

		BAN::String absolute_path;
		if (path.front() != '/')
		{
			LockGuard _(m_lock);
			TRY(absolute_path.append(m_working_directory));
		}

		if (!absolute_path.empty() && absolute_path.back() != '/')
			TRY(absolute_path.push_back('/'));

		TRY(absolute_path.append(path));
		
		return absolute_path;
	}

	BAN::ErrorOr<void> Process::validate_fd(int fd)
	{
		ASSERT(m_lock.is_locked());
		if (fd < 0 || m_open_files.size() <= (size_t)fd || !m_open_files[fd].inode)
			return BAN::Error::from_errno(EBADF);
		return {};
	}

	Process::OpenFileDescription& Process::open_file_description(int fd)
	{
		ASSERT(m_lock.is_locked());
		MUST(validate_fd(fd));
		return m_open_files[fd];
	}

	BAN::ErrorOr<int> Process::get_free_fd()
	{
		ASSERT(m_lock.is_locked());
		for (size_t fd = 0; fd < m_open_files.size(); fd++)
			if (!m_open_files[fd].inode)
				return fd;
		TRY(m_open_files.push_back({}));
		return m_open_files.size() - 1;
	}

	BAN::ErrorOr<void> Process::get_free_fd_pair(int fds[2])
	{
		ASSERT(m_lock.is_locked());
		int found = 0;
		for (size_t fd = 0; fd < m_open_files.size(); fd++)
		{
			if (!m_open_files[fd].inode)
			{
				fds[found++] = fd;
				if (found >= 2)
					return {};
			}
		}

		while (found < 2)
		{
			TRY(m_open_files.push_back({}));
			fds[found++] = m_open_files.size() - 1;
		}

		return {};
	}

}