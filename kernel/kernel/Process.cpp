#include <BAN/StringView.h>
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

	Process* Process::create_process()
	{
		static pid_t s_next_pid = 1;
		auto* process = new Process(s_next_pid++);
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
		auto* process = create_process();
		MUST(process->m_working_directory.push_back('/'));
		auto* thread = MUST(Thread::create_kernel(entry, data, process));
		process->add_thread(thread);
		register_process(process);
		return process;
	}

	BAN::ErrorOr<Process*> Process::create_userspace(BAN::StringView path)
	{
		auto* elf = TRY(LibELF::ELF::load_from_file(path));
		if (!elf->is_native())
		{
			derrorln("ELF has invalid architecture");
			delete elf;
			return BAN::Error::from_errno(EINVAL);
		}

		auto* process = create_process();
		MUST(process->m_working_directory.push_back('/'));
		process->m_page_table = BAN::UniqPtr<PageTable>::adopt(MUST(PageTable::create_userspace()));

		process->load_elf(*elf);

		char** argv = nullptr;
		{
			PageTableScope _(process->page_table());
			argv = (char**)MUST(process->allocate(sizeof(char**) * 2));
			argv[0] = (char*)MUST(process->allocate(path.size() + 1));
			memcpy(argv[0], path.data(), path.size());
			argv[0][path.size()] = '\0';
			argv[1] = nullptr;
		}

		process->m_userspace_entry.argc = 1;
		process->m_userspace_entry.argv = argv;
		process->m_userspace_entry.entry = elf->file_header_native().e_entry;

		delete elf;

		auto* thread = MUST(Thread::create_userspace(process));
		process->add_thread(thread);
		register_process(process);
		return process;
	}

	Process::Process(pid_t pid)
		: m_pid(pid)
		, m_tty(TTY::current())
	{ }

	Process::~Process()
	{
		ASSERT(m_threads.empty());
		ASSERT(m_fixed_width_allocators.empty());
		ASSERT(!m_general_allocator);
		ASSERT(m_mapped_ranges.empty());
		ASSERT(&PageTable::current() != m_page_table.ptr());

		dprintln("process {} exit", pid());
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
			exit();
	}

	void Process::exit()
	{
		m_lock.lock();
		m_exit_status.exited = true;
		while (m_exit_status.waiting > 0)
		{
			m_exit_status.semaphore.unblock();
			m_lock.unlock();
			// TODO: add proper software reschedule
			Scheduler::get().set_current_thread_sleeping(0);
		}
		m_lock.lock();

		m_threads.clear();
		for (auto& open_fd : m_open_files)
			open_fd.inode = nullptr;

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

	BAN::ErrorOr<void> Process::set_termios(const termios& termios)
	{
		LockGuard _(m_lock);
		if (m_tty == nullptr)
			return BAN::Error::from_errno(ENOTTY);
		m_tty->set_termios(termios);
		return {};
	}

	BAN::ErrorOr<Process*> Process::fork(uintptr_t rsp, uintptr_t rip)
	{
		Process* forked = create_process();

		forked->m_page_table = BAN::UniqPtr<PageTable>::adopt(MUST(PageTable::create_userspace()));

		LockGuard _(m_lock);
		forked->m_tty = m_tty;
		forked->m_working_directory = m_working_directory;

		forked->m_open_files = m_open_files;

		forked->m_userspace_entry = m_userspace_entry;

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

		return forked;
	}

	BAN::ErrorOr<void> Process::exec(BAN::StringView path, const char* const* argv, const char* const* envp)
	{
		BAN::Vector<BAN::String> str_argv;
		while (argv && *argv)
			if (str_argv.emplace_back(*argv++).is_error())
				return BAN::Error::from_errno(ENOMEM);

		BAN::Vector<BAN::String> str_envp;
		while (envp && *envp)
			if (str_envp.emplace_back(*envp++).is_error())
				return BAN::Error::from_errno(ENOMEM);

		auto* elf = TRY(LibELF::ELF::load_from_file(path));	
		if (!elf->is_native())
		{
			derrorln("ELF has invalid architecture");
			delete elf;
			return BAN::Error::from_errno(EINVAL);
		}

		LockGuard lock_guard(m_lock);

		m_fixed_width_allocators.clear();
		m_general_allocator.clear();

		for (auto* range : m_mapped_ranges)
			delete range;
		m_mapped_ranges.clear();

		m_open_files.clear();

		load_elf(*elf);

		m_userspace_entry.entry = elf->file_header_native().e_entry;

		delete elf;

		ASSERT(m_threads.size() == 1);
		ASSERT(&Process::current() == this);

		{
			PageTableScope _(page_table());
			m_userspace_entry.argv = (char**)MUST(allocate(sizeof(char**) * (str_argv.size() + 1)));
			for (size_t i = 0; i < str_argv.size(); i++)
			{
				m_userspace_entry.argv[i] = (char*)MUST(allocate(str_argv[i].size() + 1));
				memcpy(m_userspace_entry.argv[i], str_argv[i].data(), str_argv[i].size());
				m_userspace_entry.argv[i][str_argv[i].size()] = '\0';
			}
			m_userspace_entry.argv[str_argv.size()] = nullptr;
		}

		m_userspace_entry.argc = str_argv.size();

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

	BAN::ErrorOr<pid_t> Process::wait(pid_t pid, int* stat_loc, int options)
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

	void Process::load_elf(LibELF::ELF& elf)
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
				if (!page_table().is_range_free(elf_program_header.p_vaddr, elf_program_header.p_memsz))
				{
					page_table().debug_dump();
					Kernel::panic("vaddr {8H}-{8H} not free",
						elf_program_header.p_vaddr,
						elf_program_header.p_vaddr + elf_program_header.p_memsz
					);
				}
				uint8_t flags = PageTable::Flags::UserSupervisor | PageTable::Flags::Present;
				if (elf_program_header.p_flags & LibELF::PF_W)
					flags |= PageTable::Flags::ReadWrite;
				size_t page_start = elf_program_header.p_vaddr / PAGE_SIZE;
				size_t page_end = BAN::Math::div_round_up<size_t>(elf_program_header.p_vaddr + elf_program_header.p_memsz, PAGE_SIZE);

				size_t page_count = page_end - page_start + 1;
				MUST(m_mapped_ranges.push_back(VirtualRange::create(page_table(), page_start * PAGE_SIZE, page_count * PAGE_SIZE, flags)));

				{
					PageTableScope _(page_table());
					memcpy((void*)elf_program_header.p_vaddr, elf.data() + elf_program_header.p_offset, elf_program_header.p_filesz);
					memset((void*)(elf_program_header.p_vaddr + elf_program_header.p_filesz), 0, elf_program_header.p_memsz - elf_program_header.p_filesz);
				}
				break;
			}
			default:
				ASSERT_NOT_REACHED();
			}
		}
	}

	BAN::ErrorOr<int> Process::open(BAN::StringView path, int flags)
	{
		if (flags & ~(O_RDONLY | O_WRONLY | O_NOFOLLOW))
			return BAN::Error::from_errno(ENOTSUP);

		BAN::String absolute_path = TRY(absolute_path_of(path));

		auto file = TRY(VirtualFileSystem::get().file_from_absolute_path(absolute_path, !(flags & O_NOFOLLOW)));

		LockGuard _(m_lock);
		int fd = TRY(get_free_fd());
		auto& open_file_description = m_open_files[fd];
		open_file_description.inode = file.inode;
		open_file_description.path = BAN::move(file.canonical_path);
		open_file_description.offset = 0;
		open_file_description.flags = flags;

		return fd;
	}

	BAN::ErrorOr<void> Process::close(int fd)
	{
		LockGuard _(m_lock);
		TRY(validate_fd(fd));
		auto& open_file_description = this->open_file_description(fd);
		open_file_description.inode = nullptr;
		return {};
	}

	BAN::ErrorOr<size_t> Process::read(int fd, void* buffer, size_t count)
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

	BAN::ErrorOr<size_t> Process::write(int fd, const void* buffer, size_t count)
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

	BAN::ErrorOr<void> Process::seek(int fd, off_t offset, int whence)
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

		return {};
	}

	BAN::ErrorOr<off_t> Process::tell(int fd)
	{
		LockGuard _(m_lock);
		TRY(validate_fd(fd));
		return open_file_description(fd).offset;
	}

	BAN::ErrorOr<void> Process::creat(BAN::StringView path, mode_t mode)
	{
		auto absolute_path = TRY(absolute_path_of(path));

		size_t index;
		for (index = absolute_path.size(); index > 0; index--)
			if (absolute_path[index - 1] == '/')
				break;

		auto directory = absolute_path.sv().substring(0, index);
		auto file_name = absolute_path.sv().substring(index);

		auto parent_file = TRY(VirtualFileSystem::get().file_from_absolute_path(directory, true));
		TRY(parent_file.inode->create_file(file_name, mode));

		return {};
	}

	BAN::ErrorOr<void> Process::mount(BAN::StringView source, BAN::StringView target)
	{
		BAN::String absolute_source, absolute_target;
		{
			LockGuard _(m_lock);
			absolute_source = TRY(absolute_path_of(source));
			absolute_target = TRY(absolute_path_of(target));
		}
		TRY(VirtualFileSystem::get().mount(absolute_source, absolute_target));
		return {};
	}

	BAN::ErrorOr<void> Process::fstat(int fd, struct stat* out)
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

		return {};
	}

	BAN::ErrorOr<void> Process::stat(BAN::StringView path, struct stat* out, int flags)
	{
		int fd = TRY(open(path, flags));
		auto ret = fstat(fd, out);
		MUST(close(fd));
		return ret;
	}

	// FIXME: This whole API has to be rewritten
	BAN::ErrorOr<BAN::Vector<BAN::String>> Process::read_directory_entries(int fd)
	{
		OpenFileDescription open_fd_copy;

		{
			LockGuard _(m_lock);
			TRY(validate_fd(fd));
			open_fd_copy = open_file_description(fd);
		}

		auto result = TRY(open_fd_copy.inode->read_directory_entries(open_fd_copy.offset));

		{
			LockGuard _(m_lock);
			MUST(validate_fd(fd));
			open_file_description(fd).offset = open_fd_copy.offset + 1;
		}

		return result;
	}

	BAN::ErrorOr<BAN::String> Process::working_directory() const
	{
		BAN::String result;

		LockGuard _(m_lock);
		TRY(result.append(m_working_directory));
		
		return result;
	}

	BAN::ErrorOr<void> Process::set_working_directory(BAN::StringView path)
	{
		BAN::String absolute_path = TRY(absolute_path_of(path));

		auto file = TRY(VirtualFileSystem::get().file_from_absolute_path(absolute_path, true));
		if (!file.inode->mode().ifdir())
			return BAN::Error::from_errno(ENOTDIR);

		LockGuard _(m_lock);
		m_working_directory = BAN::move(file.canonical_path);

		return {};
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

	BAN::ErrorOr<void*> Process::allocate(size_t bytes)
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
				m_general_allocator = TRY(GeneralAllocator::create(page_table()));

			address = m_general_allocator->allocate(bytes);
		}

		if (address == 0)
			return BAN::Error::from_errno(ENOMEM);
		return (void*)address;
	}

	void Process::free(void* ptr)
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
				return;
			}
		}

		if (m_general_allocator && m_general_allocator->deallocate((vaddr_t)ptr))
			return;

		dwarnln("free called on pointer that was not allocated");	
	}

	void Process::termid(char* buffer) const
	{
		LockGuard _(m_lock);
		if (m_tty == nullptr)
			buffer[0] = '\0';
		strcpy(buffer, "/dev/");
		strcpy(buffer + 5, m_tty->name().data());
	}

	BAN::ErrorOr<BAN::String> Process::absolute_path_of(BAN::StringView path) const
	{
		if (path.empty())
			return working_directory();
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

}