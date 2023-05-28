#include <BAN/StringView.h>
#include <kernel/CriticalScope.h>
#include <kernel/FS/VirtualFileSystem.h>
#include <kernel/LockGuard.h>
#include <kernel/Memory/Heap.h>
#include <kernel/Memory/MMUScope.h>
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
			return BAN::Error::from_errno(EINVAL);
		}

		auto* process = create_process();
		MUST(process->m_working_directory.push_back('/'));
		MUST(process->init_stdio());
		process->m_mmu = new MMU();
		ASSERT(process->m_mmu);
		
		auto& elf_file_header = elf->file_header_native();
		for (size_t i = 0; i < elf_file_header.e_phnum; i++)
		{
			auto& elf_program_header = elf->program_header_native(i);

			switch (elf_program_header.p_type)
			{
			case LibELF::PT_NULL:
				break;
			case LibELF::PT_LOAD:
			{
				// TODO: Do some relocations or map kernel to higher half?
				ASSERT(process->mmu().is_range_free(elf_program_header.p_vaddr, elf_program_header.p_memsz));
				MMU::flags_t flags = MMU::Flags::UserSupervisor | MMU::Flags::Present;
				if (elf_program_header.p_flags & LibELF::PF_W)
					flags |= MMU::Flags::ReadWrite;
				size_t page_start = elf_program_header.p_vaddr / PAGE_SIZE;
				size_t page_end = BAN::Math::div_round_up<size_t>(elf_program_header.p_vaddr + elf_program_header.p_memsz, PAGE_SIZE);

				size_t page_count = page_end - page_start + 1;
				MUST(process->m_mapped_ranges.push_back(VirtualRange::create(process->mmu(), page_start * PAGE_SIZE, page_count * PAGE_SIZE, flags)));

				{
					MMUScope _(process->mmu());
					memcpy((void*)elf_program_header.p_vaddr, elf->data() + elf_program_header.p_offset, elf_program_header.p_filesz);
					memset((void*)(elf_program_header.p_vaddr + elf_program_header.p_filesz), 0, elf_program_header.p_memsz - elf_program_header.p_filesz);
				}
				break;
			}
			default:
				ASSERT_NOT_REACHED();
			}
		}

		char** argv = nullptr;
		{
			MMUScope _(process->mmu());
			argv = (char**)MUST(process->allocate(sizeof(char**) * 1));
			argv[0] = (char*)MUST(process->allocate(path.size() + 1));
			memcpy(argv[0], path.data(), path.size());
			argv[0][path.size()] = '\0';
		}

		auto* thread = MUST(Thread::create_userspace(elf_file_header.e_entry, process, 1, argv));
		process->add_thread(thread);

		delete elf;

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
		ASSERT(m_general_allocator == nullptr);
		ASSERT(m_mapped_ranges.empty());
		if (m_mmu)
		{
			MMU::kernel().load();
			delete m_mmu;
		}

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
		m_threads.clear();
		for (auto& open_fd : m_open_files)
			open_fd.inode = nullptr;

		// NOTE: We must unmap ranges while the mmu is still alive
		for (auto* range : m_mapped_ranges)
			delete range;
		m_mapped_ranges.clear();

		// NOTE: We must clear allocators while the mmu is still alive
		m_fixed_width_allocators.clear();
		if (m_general_allocator)
		{
			delete m_general_allocator;
			m_general_allocator = nullptr;
		}

		s_process_lock.lock();
		for (size_t i = 0; i < s_processes.size(); i++)
			if (s_processes[i] == this)
				s_processes.remove(i);
		s_process_lock.unlock();

		// FIXME: we can't assume this is the current process
		ASSERT(&Process::current() == this);
		Scheduler::get().set_current_process_done();
	}

	BAN::ErrorOr<void> Process::init_stdio()
	{
		ASSERT(m_open_files.empty());
		TRY(open("/dev/tty1", O_RDONLY)); // stdin
		TRY(open("/dev/tty1", O_WRONLY)); // stdout
		TRY(open("/dev/tty1", O_WRONLY)); // stderr
		return {};
	}

	BAN::ErrorOr<void> Process::set_termios(const termios& termios)
	{
		if (m_tty == nullptr)
			return BAN::Error::from_errno(ENOTTY);
		m_tty->set_termios(termios);
		return {};
	}

	BAN::ErrorOr<Process*> Process::fork(uintptr_t rsp, uintptr_t rip)
	{
		LockGuard _(m_lock);
		
		Process* forked = create_process();

		forked->m_tty = m_tty;
		forked->m_working_directory = m_working_directory;

		forked->m_open_files = m_open_files;

		forked->m_mmu = new MMU();
		ASSERT(forked->m_mmu);

		for (auto* mapped_range : m_mapped_ranges)
			MUST(forked->m_mapped_ranges.push_back(mapped_range->clone(forked->mmu())));

		ASSERT(m_threads.size() == 1);
		ASSERT(m_threads.front() == &Thread::current());

		//for (auto& allocator : m_fixed_width_allocators)
		//	MUST(forked->m_fixed_width_allocators.push_back(allocator.clone()));

		//if (m_general_allocator)
		//	forked->m_general_allocator = m_general_allocator->clone();

		Thread* thread = MUST(m_threads.front()->clone(forked, rsp, rip));
		forked->add_thread(thread);
		
		register_process(forked);

		return forked;
	}

	BAN::ErrorOr<int> Process::open(BAN::StringView path, int flags)
	{
		if (flags & ~O_RDWR)
			return BAN::Error::from_errno(ENOTSUP);

		BAN::String absolute_path = TRY(absolute_path_of(path));

		auto file = TRY(VirtualFileSystem::get().file_from_absolute_path(absolute_path));

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

		auto parent_file = TRY(VirtualFileSystem::get().file_from_absolute_path(directory));
		TRY(parent_file.inode->create_file(file_name, mode));

		return {};
	}

	BAN::ErrorOr<void> Process::mount(BAN::StringView source, BAN::StringView target)
	{
		auto absolute_source = TRY(absolute_path_of(source));
		auto absolute_target = TRY(absolute_path_of(target));
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

	BAN::ErrorOr<void> Process::stat(BAN::StringView path, struct stat* out)
	{
		int fd = TRY(open(path, O_RDONLY));
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

		auto file = TRY(VirtualFileSystem::get().file_from_absolute_path(absolute_path));
		if (!file.inode->mode().ifdir())
			return BAN::Error::from_errno(ENOTDIR);

		LockGuard _(m_lock);
		m_working_directory = BAN::move(file.canonical_path);

		return {};
	}

	static constexpr uint16_t next_power_of_two(uint16_t value)
	{
		value--;
		value |= value >> 1;
		value |= value >> 2;
		value |= value >> 4;
		value |= value >> 8;
		return value + 1;
	}

	BAN::ErrorOr<void*> Process::allocate(size_t bytes)
	{
		vaddr_t address = 0;

		if (bytes <= PAGE_SIZE)
		{
			// Do fixed width allocation
			size_t allocation_size = next_power_of_two(bytes);
			ASSERT(bytes <= allocation_size);

			LockGuard _(m_lock);

			bool needs_new_allocator { true };

			for (auto* allocator : m_fixed_width_allocators)
			{
				if (allocator->allocation_size() == allocation_size && allocator->allocations() < allocator->max_allocations())
				{
					address = allocator->allocate();
					needs_new_allocator = false;
				}
			}

			if (needs_new_allocator)
			{
				auto* allocator = new FixedWidthAllocator(mmu(), allocation_size);
				if (allocator == nullptr)
					return BAN::Error::from_errno(ENOMEM);
				TRY(m_fixed_width_allocators.push_back(allocator));
				address = m_fixed_width_allocators.back()->allocate();
			}
		}
		else
		{
			LockGuard _(m_lock);

			if (!m_general_allocator)
			{
				m_general_allocator = new GeneralAllocator(mmu());
				if (m_general_allocator == nullptr)
					return BAN::Error::from_errno(ENOMEM);
			}

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
			auto* allocator = m_fixed_width_allocators[i];
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