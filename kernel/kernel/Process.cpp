#include <BAN/ScopeGuard.h>
#include <BAN/Sort.h>
#include <BAN/StringView.h>
#include <kernel/ACPI/ACPI.h>
#include <kernel/ELF.h>
#include <kernel/Epoll.h>
#include <kernel/FS/DevFS/FileSystem.h>
#include <kernel/FS/EventFD.h>
#include <kernel/FS/ProcFS/FileSystem.h>
#include <kernel/FS/VirtualFileSystem.h>
#include <kernel/IDT.h>
#include <kernel/InterruptController.h>
#include <kernel/Lock/BlockableSpinLock.h>
#include <kernel/Lock/LockGuard.h>
#include <kernel/Memory/FileBackedRegion.h>
#include <kernel/Memory/Heap.h>
#include <kernel/Memory/MemoryBackedRegion.h>
#include <kernel/Process.h>
#include <kernel/Scheduler.h>
#include <kernel/Storage/StorageDevice.h>
#include <kernel/Terminal/PseudoTerminal.h>
#include <kernel/Timer/Timer.h>
#include <kernel/UserCopy.h>

#include <kernel/Banos.h>
#include <LibELF/AuxiliaryVector.h>

#include <LibInput/KeyboardLayout.h>

#include <fcntl.h>
#include <stdio.h>
#include <sys/banan-os.h>
#include <sys/eventfd.h>
#include <sys/futex.h>
#include <sys/sysmacros.h>
#include <sys/wait.h>

#pragma GCC diagnostic ignored "-Wstack-usage="

namespace Kernel
{

	static BAN::LinkedList<Process*> s_alarm_processes;
	static BAN::Vector<Process*> s_processes;
	static RecursiveSpinLock s_process_lock;

	BAN::HashMap<paddr_t, BAN::UniqPtr<Process::futex_t>> Process::s_futexes;
	Mutex Process::s_futex_lock;

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

	BAN::ErrorOr<Process*> Process::create_userspace(const Credentials& credentials, BAN::StringView path, BAN::Span<BAN::StringView> arguments)
	{
		auto* process = create_process(credentials, 0);
		BAN::ScopeGuard process_deleter([process] {
			process->m_mapped_regions.clear();
			process->m_page_table.clear();
			delete process;
		});

		process->m_working_directory = VirtualFileSystem::get().root_file();
		process->m_root_file         = VirtualFileSystem::get().root_file();

		process->m_page_table = BAN::UniqPtr<PageTable>::adopt(TRY(PageTable::create_userspace()));

		TRY(process->m_cmdline.emplace_back());
		TRY(process->m_cmdline.back().append(path));
		for (auto argument : arguments)
		{
			TRY(process->m_cmdline.emplace_back());
			TRY(process->m_cmdline.back().append(argument));
		}

		auto executable_file = TRY(process->find_file(AT_FDCWD, path.data(), O_EXEC));
		auto executable_inode = executable_file.inode;

		auto executable = TRY(ELF::load_from_inode(process->m_root_file.inode, executable_inode, process->m_credentials, process->page_table()));
		for (auto& region : executable.regions)
			TRY(process->m_mapped_regions.push_back(BAN::move(region)));
		executable.regions.clear();

		TRY(process->m_executable.append(executable_file.canonical_path));

		if (executable_inode->mode().mode & +Inode::Mode::ISUID)
			process->m_credentials.set_euid(executable_inode->uid());
		if (executable_inode->mode().mode & +Inode::Mode::ISGID)
			process->m_credentials.set_egid(executable_inode->gid());

		process->m_shared_page_vaddr = process->page_table().reserve_free_page(executable.interp_base.value_or(0x400000), USERSPACE_END);
		if (process->m_shared_page_vaddr == 0)
			return BAN::Error::from_errno(ENOMEM);
		process->page_table().map_page_at(
			Processor::shared_page_paddr(),
			process->m_shared_page_vaddr,
			PageTable::UserSupervisor | PageTable::Present
		);

		auto userspace_stack = TRY(MemoryBackedRegion::create(
			process->page_table(),
			Thread::userspace_stack_size,
			{ Thread::userspace_stack_base, USERSPACE_END },
			MemoryRegion::Type::PRIVATE,
			PageTable::UserSupervisor | PageTable::ReadWrite | PageTable::Present,
			O_RDWR
		));

		BAN::Vector<LibELF::AuxiliaryVector> auxiliary_vector;
		TRY(auxiliary_vector.reserve(5 + 2 * executable.interp_base.has_value()));

		if (executable.interp_base.has_value())
		{
			const int execfd = TRY(process->m_open_file_descriptors.open(BAN::move(executable_file), O_RDONLY));
			TRY(auxiliary_vector.push_back({
				.a_type = LibELF::AT_EXECFD,
				.a_un = { .a_val = static_cast<uint32_t>(execfd) },
			}));
			TRY(auxiliary_vector.push_back({
				.a_type = LibELF::AT_BASE,
				.a_un = { .a_ptr = reinterpret_cast<void*>(executable.interp_base.value()) },
			}));
		}

		TRY(auxiliary_vector.push_back({
			.a_type = LibELF::AT_PAGESZ,
			.a_un = { .a_val = PAGE_SIZE },
		}));

		TRY(auxiliary_vector.push_back({
			.a_type = LibELF::AT_SHARED_PAGE,
			.a_un = { .a_ptr = reinterpret_cast<void*>(process->m_shared_page_vaddr) },
		}));

		TRY(auxiliary_vector.push_back({
			.a_type = LibELF::AT_STACK_BASE,
			.a_un = { .a_ptr = reinterpret_cast<void*>(userspace_stack->vaddr()) },
		}));

		TRY(auxiliary_vector.push_back({
			.a_type = LibELF::AT_STACK_SIZE,
			.a_un = { .a_ptr = reinterpret_cast<void*>(userspace_stack->size()) },
		}));

		TRY(auxiliary_vector.push_back({
			.a_type = LibELF::AT_NULL,
			.a_un = { .a_val = 0 },
		}));

		const vaddr_t initial_stack_pointer = TRY(process->setup_initial_process_stack(
			*userspace_stack,
			process->m_cmdline.span(),
			process->m_environ.span(),
			auxiliary_vector.span()
		));

		const vaddr_t userspace_stack_vaddr = userspace_stack->vaddr();
		const vaddr_t userspace_stack_size  = userspace_stack->size();
		TRY(process->m_mapped_regions.push_back(BAN::move(userspace_stack)));

		BAN::Optional<vaddr_t> tls_addr;
		if (executable.master_tls.has_value())
		{
			auto tls_result = TRY(process->initialize_thread_local_storage(process->page_table(), *executable.master_tls));
			TRY(process->m_mapped_regions.push_back(BAN::move(tls_result.region)));
			tls_addr = tls_result.addr;
		}

		BAN::sort::sort(process->m_mapped_regions.begin(), process->m_mapped_regions.end(), [](const auto& a, const auto& b) {
			return a->vaddr() < b->vaddr();
		});

		auto* thread = TRY(Thread::create_userspace(
			process,
			process->page_table(),
			userspace_stack_vaddr,
			userspace_stack_size,
			executable.entry_point,
			initial_stack_pointer
		));
		BAN::ScopeGuard thread_deleter([thread] { delete thread; });

		if (tls_addr.has_value())
		{
#if ARCH(x86_64)
			thread->set_fsbase(*tls_addr);
#elif ARCH(i686)
			thread->set_gsbase(*tls_addr);
#else
#error
#endif
		}

		// NOTE: make sure the last two `MUST`s don't fail
		TRY(process->m_threads.reserve(1));
		TRY(Processor::scheduler().bind_thread_to_processor(thread, Processor::current_id()));

		{
			SpinLockGuard _(s_process_lock);
			TRY(s_processes.push_back(process));
		}

		MUST(process->m_threads.push_back(thread));
		MUST(Processor::scheduler().add_thread(thread));

		process_deleter.disable();
		thread_deleter.disable();

		return process;
	}

	BAN::ErrorOr<vaddr_t> Process::setup_initial_process_stack(MemoryBackedRegion& stack_region, BAN::Span<BAN::String> argv, BAN::Span<BAN::String> envp, BAN::Span<LibELF::AuxiliaryVector> auxv)
	{
		// System V ABI: Initial process stack

		size_t needed_size = 0;

		// argc
		needed_size += sizeof(uintptr_t);

		// argv
		needed_size += (argv.size() + 1) * sizeof(uintptr_t);
		for (auto arg : argv)
			needed_size += arg.size() + 1;

		// envp
		needed_size += (envp.size() + 1) * sizeof(uintptr_t);
		for (auto env : envp)
			needed_size += env.size() + 1;

		// auxv
		needed_size += auxv.size() * sizeof(LibELF::AuxiliaryVector);

		if (auto rem = needed_size % alignof(char*))
			needed_size += alignof(char*) - rem;

		if (needed_size > stack_region.size())
			return BAN::Error::from_errno(ENOBUFS);

		vaddr_t vaddr = stack_region.vaddr() + stack_region.size() - needed_size;

		const size_t page_count = BAN::Math::div_round_up<size_t>(needed_size, PAGE_SIZE);
		for (size_t i = 0; i < page_count; i++)
			TRY(stack_region.allocate_page_containing(vaddr + i * PAGE_SIZE, true));

		const auto stack_copy_buf =
			[&stack_region](BAN::ConstByteSpan buffer, vaddr_t vaddr) -> void
			{
				ASSERT(vaddr >= stack_region.vaddr());
				ASSERT(vaddr + buffer.size() <= stack_region.vaddr() + stack_region.size());
				MUST(stack_region.copy_data_to_region(vaddr - stack_region.vaddr(), buffer.data(), buffer.size()));
			};

		const auto stack_push_buf =
			[&stack_copy_buf, &vaddr](BAN::ConstByteSpan buffer) -> void
			{
				stack_copy_buf(buffer, vaddr);
				vaddr += buffer.size();
			};

		const auto stack_push_uint =
			[&stack_push_buf](uintptr_t value) -> void
			{
				stack_push_buf(BAN::ConstByteSpan::from(value));
			};

		const auto stack_push_str =
			[&stack_push_buf](BAN::StringView string) -> void
			{
				const uint8_t* string_u8 = reinterpret_cast<const uint8_t*>(string.data());
				stack_push_buf(BAN::ConstByteSpan(string_u8, string.size() + 1));
			};

		// argc
		stack_push_uint(argv.size());

		// argv
		const vaddr_t argv_vaddr = vaddr;
		vaddr += argv.size() * sizeof(uintptr_t);
		stack_push_uint(0);

		// envp
		const vaddr_t envp_vaddr = vaddr;
		vaddr += envp.size() * sizeof(uintptr_t);
		stack_push_uint(0);

		// auxv
		for (auto aux : auxv)
			stack_push_buf(BAN::ConstByteSpan::from(aux));

		// information
		for (size_t i = 0; i < argv.size(); i++)
		{
			stack_copy_buf(BAN::ConstByteSpan::from(vaddr), argv_vaddr + i * sizeof(uintptr_t));
			stack_push_str(argv[i]);
		}
		for (size_t i = 0; i < envp.size(); i++)
		{
			stack_copy_buf(BAN::ConstByteSpan::from(vaddr), envp_vaddr + i * sizeof(uintptr_t));
			stack_push_str(envp[i]);
		}

		return stack_region.vaddr() + stack_region.size() - needed_size;
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
		ASSERT(m_exited_threads.empty());
		ASSERT(m_mapped_regions.empty());
		ASSERT(!m_page_table);
	}

	bool Process::is_pgrpg_in_this_session(pid_t pgrp) const
	{
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
		return valid_pgrp;
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

		m_exited_threads.clear();

		ProcFileSystem::get().on_process_delete(*this);

		m_process_lock.lock();
		m_memory_region_lock.wr_lock();

		m_open_file_descriptors.close_all();

		// NOTE: We must unmap ranges while the page table is still alive
		m_mapped_regions.clear();

		// After we give our page table to the thread, we cannot get rescheduled
		Processor::set_interrupt_state(InterruptState::Disabled);
		thread->give_keep_alive_page_table(BAN::move(m_page_table));
	}

	bool Process::on_thread_exit(Thread& thread)
	{
		// TODO: if main thread exists, should we delete its stack?

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
		ASSERT(Processor::get_interrupt_state() == InterruptState::Enabled);

		bool expected = false;
		if (!m_is_exiting.compare_exchange(expected, true))
		{
			Thread::current().on_exit();
			ASSERT_NOT_REACHED();
		}

		if (m_parent)
		{
			Process* parent_process = nullptr;

			for_each_process(
				[&](Process& parent) -> BAN::Iteration
				{
					if (parent.pid() != m_parent)
						return BAN::Iteration::Continue;
					parent_process = &parent;
					return BAN::Iteration::Break;
				}
			);

			if (parent_process)
			{
				SpinLockGuard _(parent_process->m_child_wait_lock);

				for (auto& child : parent_process->m_child_wait_statuses)
				{
					if (child.pid != pid())
						continue;

					child.status = __WGENEXITCODE(status, signal);

					parent_process->add_pending_signal(SIGCHLD, {
						.si_signo = SIGCHLD,
						.si_code = signal ? CLD_KILLED : CLD_EXITED,
						.si_errno = 0,
						.si_pid = pid(),
						.si_uid = m_credentials.ruid(),
						.si_addr = nullptr,
						.si_status = __WGENEXITCODE(status, signal),
						.si_band = 0,
						.si_value = {},
					});
					if (!parent_process->m_threads.empty())
						Processor::scheduler().unblock_thread(parent_process->m_threads.front());

					parent_process->m_child_wait_blocker.unblock();

					break;
				}
			}
		}

		{
			LockGuard _(m_process_lock);
			for (auto* thread : m_threads)
				if (thread != &Thread::current())
					thread->add_signal(SIGKILL, {});
		}

		while (m_threads.size() > 1)
			Processor::yield();

		Thread::current().on_exit();

		ASSERT_NOT_REACHED();
	}

	BAN::ErrorOr<Process::TLSResult> Process::initialize_thread_local_storage(PageTable& page_table, ELF::LoadResult::TLS master_tls)
	{
		const auto [master_addr, master_size] = master_tls;
		ASSERT(master_size % alignof(uthread) == 0);

		const size_t tls_size = master_size + sizeof(uthread);

		auto region = TRY(MemoryBackedRegion::create(
			page_table,
			tls_size,
			{ .start = master_addr, .end = USERSPACE_END },
			MemoryRegion::Type::PRIVATE,
			PageTable::Flags::UserSupervisor | PageTable::Flags::ReadWrite | PageTable::Flags::Present,
			O_EXEC | O_RDWR
		));

		size_t bytes_copied = 0;
		while (bytes_copied < master_size)
		{
			uint8_t buffer[PAGE_SIZE];
			const size_t to_copy = BAN::Math::min(master_size - bytes_copied, sizeof(buffer));

			const vaddr_t vaddr = master_addr + bytes_copied;
			const paddr_t paddr = page_table.physical_address_of(vaddr & PAGE_ADDR_MASK);
			PageTable::with_fast_page(paddr, [&] {
				memcpy(buffer, PageTable::fast_page_as_ptr(vaddr % PAGE_SIZE), to_copy);
			});

			TRY(region->copy_data_to_region(bytes_copied, buffer, to_copy));
			bytes_copied += to_copy;
		}

		uthread uthread {};
		uthread.self = reinterpret_cast<struct uthread*>(region->vaddr() + master_size);
		uthread.master_tls_addr = reinterpret_cast<void*>(master_addr),
		uthread.master_tls_size = master_size,
		uthread.master_tls_module_count = 1;
		uthread.dynamic_tls = nullptr;
		uthread.dtv[0] = 0;
		uthread.dtv[1] = region->vaddr();

		TRY(region->copy_data_to_region(
			master_size,
			reinterpret_cast<const uint8_t*>(&uthread),
			sizeof(struct uthread)
		));

		return TLSResult {
			.region = BAN::move(region),
			.addr   = region->vaddr() + master_size,
		};
	}

	BAN::ErrorOr<void> Process::add_mapped_region(BAN::UniqPtr<MemoryRegion>&& region)
	{
		const size_t index = find_mapped_region(region->vaddr());
		TRY(m_mapped_regions.insert(index, BAN::move(region)));
		return {};
	}

	size_t Process::find_mapped_region(vaddr_t address) const
	{
		size_t l = 0, r = m_mapped_regions.size();

		while (l < r)
		{
			const size_t mid = (l + r) / 2;

			if (m_mapped_regions[mid]->contains(address))
				return mid;

			if (m_mapped_regions[mid]->vaddr() < address)
				l = mid + 1;
			else
				r = mid;
		}

		return l;
	}

	BAN::ErrorOr<AddressRange> Process::find_free_address_range(size_t size)
	{
		if (auto rem = size % PAGE_SIZE)
			size += PAGE_SIZE - rem;

		vaddr_t vaddr = m_shared_page_vaddr ? m_shared_page_vaddr + PAGE_SIZE : 0x400000;
		for (size_t i = find_mapped_region(vaddr); i < m_mapped_regions.size(); i++)
		{
			const auto& region = m_mapped_regions[i];
			if (!region->overlaps(vaddr, size))
				return AddressRange { vaddr, vaddr + size };

			vaddr = region->vaddr() + region->size();
			if (auto rem = vaddr % PAGE_SIZE)
				vaddr += PAGE_SIZE - rem;
		}

		return BAN::Error::from_errno(ENOMEM);
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
		}
		{
			RWLockRDGuard _(m_memory_region_lock);
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

	size_t Process::proc_cputime(off_t offset, BAN::ByteSpan buffer) const
	{
		uint64_t user_ns { 0 }, system_ns { 0 };

		{
			LockGuard _(m_process_lock);
			for (auto* thread : m_threads)
			{
				uint64_t u, s;
				thread->cpu_time_ns(u, s);
				user_ns   += u;
				system_ns += s;
			}
		}

		auto data = MUST(BAN::String::formatted("{} {} {}", user_ns + system_ns, user_ns, system_ns));
		if (static_cast<size_t>(offset) >= data.size())
			return 0;

		const size_t to_copy = BAN::Math::min<size_t>(data.size() - offset, buffer.size());
		memcpy(buffer.data(), data.data(), to_copy);
		return to_copy;
	}

	BAN::ErrorOr<BAN::String> Process::proc_cwd() const
	{
		LockGuard _(m_process_lock);
		BAN::String result;
		TRY(result.append(m_working_directory.canonical_path));
		return result;
	}

	BAN::ErrorOr<BAN::String> Process::proc_exe() const
	{
		LockGuard _(m_process_lock);
		BAN::String result;
		TRY(result.append(m_executable));
		return result;
	}

	BAN::ErrorOr<VirtualFileSystem::File> Process::find_file(int fd, const char* path, int flags) const
	{
		LockGuard _(m_process_lock);

		auto parent_file = TRY(find_relative_parent(fd, path));
		auto file = path
			? TRY(VirtualFileSystem::get().file_from_relative_path(m_root_file.inode, parent_file, m_credentials, path, flags))
			: BAN::move(parent_file);

		return file;
	}

	BAN::ErrorOr<Process::FileParent> Process::find_parent_file(int fd, const char* path, int flags) const
	{
		LockGuard _(m_process_lock);

		if (path && path[0] == '\0')
			return BAN::Error::from_errno(ENOENT);

		auto relative_parent = TRY(find_relative_parent(fd, path));

		VirtualFileSystem::File parent;
		BAN::StringView file_name;

		auto path_sv = path ? BAN::StringView(path) : ""_sv;
		while (!path_sv.empty() && path_sv.back() == '/')
			path_sv = path_sv.substring(0, path_sv.size() - 1);

		if (auto index = path_sv.rfind('/'); index.has_value())
		{
			parent = TRY(VirtualFileSystem::get().file_from_relative_path(m_root_file.inode, relative_parent, m_credentials, path_sv.substring(0, index.value()), flags));
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
			return TRY(m_root_file.clone());

		if (fd == AT_FDCWD)
			return TRY(m_working_directory.clone());

		return TRY(m_open_file_descriptors.file_of(fd));
	}

	BAN::ErrorOr<long> Process::sys_exit(int status)
	{
		ASSERT(this == &Process::current());
		exit(status, 0);
		ASSERT_NOT_REACHED();
	}

	BAN::ErrorOr<long> Process::sys_fork(uintptr_t sp, uintptr_t ip)
	{
		auto page_table = BAN::UniqPtr<PageTable>::adopt(TRY(PageTable::create_userspace()));

		LockGuard _(m_process_lock);

		ChildWaitStatus* child_exit_status = nullptr;

		{
			SpinLockGuard _(m_child_wait_lock);
			for (auto& child : m_child_wait_statuses)
			{
				if (child.pid != 0)
					continue;
				child_exit_status = &child;
				break;
			}
			if (child_exit_status == nullptr)
			{
				TRY(m_child_wait_statuses.emplace_back());
				child_exit_status = &m_child_wait_statuses.back();
			}
		}

		auto working_directory = TRY(m_working_directory.clone());
		auto root_file         = TRY(m_root_file.clone());

		BAN::String executable;
		TRY(executable.append(m_executable));

		BAN::Vector<BAN::String> cmdline;
		TRY(cmdline.resize(m_cmdline.size()));
		for (size_t i = 0; i < m_cmdline.size(); i++)
			TRY(cmdline[i].append(m_cmdline[i]));

		BAN::Vector<BAN::String> environ;
		TRY(environ.resize(m_environ.size()));
		for (size_t i = 0; i < m_environ.size(); i++)
			TRY(environ[i].append(m_environ[i]));

		auto open_file_descriptors = TRY(BAN::UniqPtr<OpenFileDescriptorSet>::create(m_credentials));
		TRY(open_file_descriptors->clone_from(m_open_file_descriptors));

		BAN::Vector<BAN::UniqPtr<MemoryRegion>> mapped_regions;
		{
			RWLockRDGuard _(m_memory_region_lock);
			TRY(mapped_regions.reserve(m_mapped_regions.size()));
			for (auto& mapped_region : m_mapped_regions)
				MUST(mapped_regions.push_back(TRY(mapped_region->clone(*page_table))));
		}

		const vaddr_t shared_page_vaddr = m_shared_page_vaddr;
		page_table->map_page_at(
			Processor::shared_page_paddr(),
			shared_page_vaddr,
			PageTable::UserSupervisor | PageTable::Present
		);

		Process* forked = create_process(m_credentials, m_pid, m_sid, m_pgrp);
		BAN::ScopeGuard process_deleter([forked] {
			forked->m_page_table.clear();
			delete forked;
		});
		forked->m_page_table = BAN::move(page_table);

		Thread* thread = TRY(Thread::current().clone(forked, sp, ip));
		BAN::ScopeGuard thread_deleter([thread] { delete thread; });

		// NOTE: make sure the last two `MUST`s don't fail
		TRY(forked->m_threads.reserve(1));
		TRY(Processor::scheduler().bind_thread_to_processor(thread, Processor::current_id()));

		{
			SpinLockGuard _(s_process_lock);
			TRY(s_processes.push_back(forked));
		}

		forked->m_controlling_terminal = m_controlling_terminal;
		forked->m_working_directory = BAN::move(working_directory);
		forked->m_root_file = BAN::move(root_file);
		forked->m_cmdline = BAN::move(cmdline);
		forked->m_environ = BAN::move(environ);
		forked->m_executable = BAN::move(executable);
		forked->m_shared_page_vaddr = BAN::move(shared_page_vaddr);
		forked->m_open_file_descriptors = BAN::move(*open_file_descriptors);
		forked->m_mapped_regions = BAN::move(mapped_regions);
		forked->m_has_called_exec = false;
		memcpy(forked->m_signal_handlers, m_signal_handlers, sizeof(m_signal_handlers));

		*child_exit_status = {};
		child_exit_status->pid = forked->pid();
		child_exit_status->pgrp = forked->pgrp();

		ASSERT(this == &Process::current());

		MUST(forked->m_threads.push_back(thread));
		MUST(Processor::scheduler().add_thread(thread));

		process_deleter.disable();
		thread_deleter.disable();

		return forked->pid();
	}

	BAN::ErrorOr<long> Process::sys_exec(const char* user_path, const char* const* user_argv, const char* const* user_envp)
	{
		// NOTE: We scope everything for automatic deletion
		{
			LockGuard _(m_process_lock);

			auto new_page_table = BAN::UniqPtr<PageTable>::adopt(TRY(PageTable::create_userspace()));

			char path[PATH_MAX];
			TRY(read_string_from_user(user_path, path, PATH_MAX));

			BAN::Vector<char> arg_buffer;
			TRY(arg_buffer.resize(ARG_MAX));

			BAN::Vector<BAN::String> str_argv;
			for (int i = 0; user_argv; i++)
			{
				char* argv_i;
				TRY(read_from_user(user_argv + i, &argv_i, sizeof(char*)));
				if (argv_i == nullptr)
					break;

				BAN::String arg;
				TRY(read_string_from_user(argv_i, arg_buffer.data(), arg_buffer.size()));
				TRY(arg.append(BAN::StringView { arg_buffer.data() }));
				TRY(str_argv.emplace_back(BAN::move(arg)));
			}

			BAN::Vector<BAN::String> str_envp;
			for (int i = 0; user_envp; i++)
			{
				char* envp_i;
				TRY(read_from_user(user_envp + i, &envp_i, sizeof(char*)));
				if (envp_i == nullptr)
					break;

				BAN::String env;
				TRY(read_string_from_user(envp_i, arg_buffer.data(), arg_buffer.size()));
				TRY(env.append(BAN::StringView { arg_buffer.data() }));
				TRY(str_envp.emplace_back(BAN::move(env)));
			}

			auto executable_file = TRY(find_file(AT_FDCWD, path, O_EXEC));
			auto executable_inode = executable_file.inode;

			auto executable = TRY(ELF::load_from_inode(m_root_file.inode, executable_inode, m_credentials, *new_page_table));
			auto new_mapped_regions = BAN::move(executable.regions);

			BAN::String executable_path;
			TRY(executable_path.append(executable_file.canonical_path));

			const vaddr_t shared_page_vaddr = new_page_table->reserve_free_page(executable.interp_base.value_or(0x400000), USERSPACE_END);
			if (shared_page_vaddr == 0)
				return BAN::Error::from_errno(ENOMEM);
			new_page_table->map_page_at(
				Processor::shared_page_paddr(),
				shared_page_vaddr,
				PageTable::UserSupervisor | PageTable::Present
			);

			auto userspace_stack = TRY(MemoryBackedRegion::create(
				*new_page_table,
				Thread::userspace_stack_size,
				{ Thread::userspace_stack_base, USERSPACE_END },
				MemoryRegion::Type::PRIVATE,
				PageTable::UserSupervisor | PageTable::ReadWrite | PageTable::Present,
				O_RDWR
			));

			BAN::Vector<LibELF::AuxiliaryVector> auxiliary_vector;
			TRY(auxiliary_vector.reserve(5 + 2 * executable.interp_base.has_value()));

			BAN::ScopeGuard execfd_guard([this, &auxiliary_vector] {
				if (auxiliary_vector.empty())
					return;
				if (auxiliary_vector.front().a_type != LibELF::AT_EXECFD)
					return;
				MUST(m_open_file_descriptors.close(auxiliary_vector.front().a_un.a_val));
			});

			if (executable.interp_base.has_value())
			{
				const int execfd = TRY(m_open_file_descriptors.open(BAN::move(executable_file), O_RDONLY));
				TRY(auxiliary_vector.push_back({
					.a_type = LibELF::AT_EXECFD,
					.a_un = { .a_val = static_cast<uint32_t>(execfd) },
				}));
				TRY(auxiliary_vector.push_back({
					.a_type = LibELF::AT_BASE,
					.a_un = { .a_ptr = reinterpret_cast<void*>(executable.interp_base.value()) },
				}));
			}
			TRY(auxiliary_vector.push_back({
				.a_type = LibELF::AT_PAGESZ,
				.a_un = { .a_val = PAGE_SIZE },
			}));

			TRY(auxiliary_vector.push_back({
				.a_type = LibELF::AT_SHARED_PAGE,
				.a_un = { .a_ptr = reinterpret_cast<void*>(shared_page_vaddr) },
			}));

			TRY(auxiliary_vector.push_back({
				.a_type = LibELF::AT_STACK_BASE,
				.a_un = { .a_ptr = reinterpret_cast<void*>(userspace_stack->vaddr()) },
			}));

			TRY(auxiliary_vector.push_back({
				.a_type = LibELF::AT_STACK_SIZE,
				.a_un = { .a_ptr = reinterpret_cast<void*>(userspace_stack->size()) },
			}));

			TRY(auxiliary_vector.push_back({
				.a_type = LibELF::AT_NULL,
				.a_un = { .a_val = 0 },
			}));

			const vaddr_t initial_stack_pointer = TRY(setup_initial_process_stack(
				*userspace_stack,
				str_argv.span(),
				str_envp.span(),
				auxiliary_vector.span()
			));

			const vaddr_t userspace_stack_vaddr = userspace_stack->vaddr();
			const vaddr_t userspace_stack_size  = userspace_stack->size();
			TRY(new_mapped_regions.emplace_back(BAN::move(userspace_stack)));

			BAN::Optional<vaddr_t> tls_addr;
			if (executable.master_tls.has_value())
			{
				auto tls_result = TRY(initialize_thread_local_storage(*new_page_table, *executable.master_tls));
				TRY(new_mapped_regions.emplace_back(BAN::move(tls_result.region)));
				tls_addr = tls_result.addr;
			}

			BAN::sort::sort(new_mapped_regions.begin(), new_mapped_regions.end(), [](auto& a, auto& b) {
				return a->vaddr() < b->vaddr();
			});

			auto* new_thread = TRY(Thread::create_userspace(
				this,
				*new_page_table,
				userspace_stack_vaddr,
				userspace_stack_size,
				executable.entry_point,
				initial_stack_pointer
			));
			if (tls_addr.has_value())
			{
#if ARCH(x86_64)
				new_thread->set_fsbase(tls_addr.value());
#elif ARCH(i686)
				new_thread->set_gsbase(tls_addr.value());
#else
#error
#endif
			}

			// NOTE: bind new thread to this processor so it wont be rescheduled before end of this function
			//       and so that adding the thread to the scheduler cannot fail
			if (auto ret = Scheduler::bind_thread_to_processor(new_thread, Processor::current_id()); ret.is_error())
			{
				Processor::set_interrupt_state(InterruptState::Enabled);
				delete new_thread;
				return ret.release_error();
			}

			RWLockWRGuard wr_guard(m_memory_region_lock);

			// NOTE: this is done before disabling interrupts and moving the threads as
			//       shared filebacked mmap can write to disk on on clearing, this will lock
			//       filesystem mutex which can yield
			m_mapped_regions.clear();

			ASSERT(Processor::get_interrupt_state() == InterruptState::Enabled);
			Processor::set_interrupt_state(InterruptState::Disabled);

			// after this point, everything is initialized and nothing can fail!

			ASSERT(m_threads.size() == 1);
			ASSERT(&Thread::current() == m_threads.front());

			// Make current thread standalone and terminated
			// We need to give it the current page table to keep it alive
			// while its kernel stack is in use
			m_threads.front()->m_state = Thread::State::Terminated;
			m_threads.front()->m_process = nullptr;
			m_threads.front()->give_keep_alive_page_table(BAN::move(m_page_table));

			MUST(Processor::scheduler().add_thread(new_thread));
			m_threads.front() = new_thread;

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

			m_shared_page_vaddr = shared_page_vaddr;
			m_threads.front()->update_processor_index_address();

			execfd_guard.disable();

			m_cmdline = BAN::move(str_argv);
			m_environ = BAN::move(str_envp);
			m_executable = BAN::move(executable_path);
		}

		m_has_called_exec = true;
		Processor::yield();
		ASSERT_NOT_REACHED();
	}

	BAN::ErrorOr<long> Process::sys_wait(pid_t pid, int* user_stat_loc, int options)
	{
		if (options & ~(WCONTINUED | WNOHANG | WUNTRACED))
			return BAN::Error::from_errno(EINVAL);

		// FIXME: Add WCONTINUED and WUNTRACED when stopped/continued processes are added

		const auto pid_matches =
			[&](const ChildWaitStatus& child)
			{
				if (pid == -1)
					return true;
				if (pid == 0)
					return child.pgrp == pgrp();
				if (pid < 0)
					return child.pgrp == -pid;
				return child.pid == pid;
			};

		pid_t child_pid = 0;
		int child_status = 0;

		for (;;)
		{
			bool found = false;

			SpinLockGuard sguard(m_child_wait_lock);

			for (auto& child : m_child_wait_statuses)
			{
				if (!pid_matches(child))
					continue;

				found = true;
				if (!child.status.has_value())
					continue;

				const int status = child.status.value();

				bool should_report = false;
				if (WIFSTOPPED(status))
					should_report = !!(options & WUNTRACED);
				else if (WIFCONTINUED(status))
					should_report = !!(options & WCONTINUED);
				else
					should_report = true;

				if (!should_report)
					continue;

				child_pid = child.pid;
				child_status = status;
				child.status = {};

				break;
			}

			if (child_pid != 0)
				break;

			if (!found)
				return BAN::Error::from_errno(ECHILD);

			if (options & WNOHANG)
				return 0;

			BlockableSpinLock block(m_child_wait_lock);
			TRY(Thread::current().block_or_eintr_indefinite(m_child_wait_blocker, &block));
		}

		if (user_stat_loc != nullptr)
			TRY(write_to_user(user_stat_loc, &child_status, sizeof(int)));

		remove_pending_signal(SIGCHLD);
		return child_pid;
	}

	BAN::ErrorOr<long> Process::sys_nanosleep(const timespec* user_rqtp, timespec* user_rmtp)
	{
		// take current timestamp ASAP
		uint64_t current_ns = SystemTimer::get().ns_since_boot();

		timespec rqtp;
		TRY(read_from_user(user_rqtp, &rqtp, sizeof(timespec)));

		if (rqtp.tv_sec < 0 || rqtp.tv_nsec < 0 || rqtp.tv_nsec >= 1'000'000'000)
			return BAN::Error::from_errno(EINVAL);

		const uint64_t waketime_ns = [&rqtp, current_ns]() -> uint64_t {
			if (BAN::Math::will_multiplication_overflow<uint64_t>(rqtp.tv_sec, 1'000'000'000))
				return BAN::numeric_limits<uint64_t>::max();
			if (BAN::Math::will_addition_overflow<uint64_t>(rqtp.tv_sec * 1'000'000'000, rqtp.tv_nsec))
				return BAN::numeric_limits<uint64_t>::max();
			if (BAN::Math::will_addition_overflow<uint64_t>(rqtp.tv_sec * 1'000'000'000 + rqtp.tv_nsec, current_ns))
				return BAN::numeric_limits<uint64_t>::max();
			return rqtp.tv_sec * 1'000'000'000 + rqtp.tv_nsec + current_ns;
		}();

		// NOTE: we shouldn't get woken up unless the thread is interrupted but there is no harm in looping
		while (current_ns < waketime_ns && !Thread::current().is_interrupted_by_signal())
		{
			SystemTimer::get().sleep_until_ns(waketime_ns);
			current_ns = SystemTimer::get().ns_since_boot();
		}

		if (current_ns < waketime_ns)
		{
			if (user_rmtp != nullptr)
			{
				const uint64_t remaining_ns = waketime_ns - current_ns;
				const timespec remaining_ts = {
					.tv_sec = static_cast<time_t>(remaining_ns / 1'000'000'000),
					.tv_nsec = static_cast<long>(remaining_ns % 1'000'000'000),
				};
				TRY(write_to_user(user_rmtp, &remaining_ts, sizeof(timespec)));
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

		MemoryRegion* value_region = nullptr;
		MemoryRegion* ovalue_region = nullptr;
		BAN::ScopeGuard _([&] {
			if (value_region)
				value_region->unpin();
			if (ovalue_region)
				ovalue_region->unpin();
		});

		value_region = TRY(validate_and_pin_pointer_access(value, sizeof(itimerval), false));
		if (ovalue != nullptr)
			ovalue_region = TRY(validate_and_pin_pointer_access(ovalue, sizeof(itimerval), true));

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
		ASSERT(Processor::current_is_bsp());

		SpinLockGuard _(s_process_lock);

		const uint64_t current_ns = SystemTimer::get().ns_since_boot();

		while (!s_alarm_processes.empty())
		{
			auto* process = s_alarm_processes.front();
			if (current_ns < process->m_alarm_wake_time_ns)
				break;

			process->add_pending_signal(SIGALRM, {
				.si_signo = SIGALRM,
				.si_code = SI_TIMER,
				.si_errno = 0,
				.si_pid = 0,
				.si_uid = 0,
				.si_addr = nullptr,
				.si_status = 0,
				.si_band = 0,
				.si_value = {},
			});

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

	BAN::ErrorOr<void> Process::create_file(int fd, const char* path, mode_t mode) const
	{
		switch (mode & Inode::Mode::TYPE_MASK)
		{
			case Inode::Mode::IFREG:
			case Inode::Mode::IFLNK:
			case Inode::Mode::IFIFO:
			case Inode::Mode::IFSOCK:
				break;
			default:
				ASSERT_NOT_REACHED();
		}

		struct uid_gid_t { uid_t uid; gid_t gid; };
		const auto [uid, gid] = ({
			LockGuard _(m_process_lock);
			uid_gid_t { m_credentials.euid(), m_credentials.egid() };
		});

		auto [parent, file_name] = TRY(find_parent_file(fd, path, O_EXEC | O_WRONLY));
		TRY(parent.inode->create_file(file_name, mode, uid, gid));

		return {};
	}

	BAN::ErrorOr<long> Process::open_inode(VirtualFileSystem::File&& file, int flags)
	{
		ASSERT(file.inode);
		LockGuard _(m_process_lock);
		return TRY(m_open_file_descriptors.open(BAN::move(file), flags));
	}

	BAN::ErrorOr<long> Process::sys_openat(int fd, const char* user_path, int flags, mode_t mode)
	{
		if ((flags & (O_DIRECTORY | O_CREAT)) == (O_DIRECTORY | O_CREAT))
			return BAN::Error::from_errno(EINVAL);

		char path[PATH_MAX];
		TRY(read_string_from_user(user_path, path, PATH_MAX));

		LockGuard _(m_process_lock);

		auto [parent, file_name] = TRY(find_parent_file(fd, path, O_RDONLY));
		auto file_or_error = VirtualFileSystem::get().file_from_relative_path(m_root_file.inode, parent, m_credentials, file_name, flags);

		VirtualFileSystem::File file;
		if (file_or_error.is_error())
		{
			if (!(flags & O_CREAT) || file_or_error.error().get_error_code() != ENOENT)
				return file_or_error.release_error();

			// FIXME: There is a race condition between next two lines
			TRY(parent.inode->create_file(file_name, (mode & 0777) | Inode::Mode::IFREG, m_credentials.euid(), m_credentials.egid()));
			file = TRY(VirtualFileSystem::get().file_from_relative_path(m_root_file.inode, parent, m_credentials, file_name, flags & ~O_RDWR));
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
		TRY(m_open_file_descriptors.close(fd));
		return 0;
	}

	BAN::ErrorOr<long> Process::sys_read(int fd, void* buffer, size_t count)
	{
		if (count == 0)
		{
			TRY(m_open_file_descriptors.inode_of(fd));
			return 0;
		}

		auto* buffer_region = TRY(validate_and_pin_pointer_access(buffer, count, true));
		BAN::ScopeGuard _([buffer_region] { buffer_region->unpin(); });
		return TRY(m_open_file_descriptors.read(fd, BAN::ByteSpan(static_cast<uint8_t*>(buffer), count)));
	}

	BAN::ErrorOr<long> Process::sys_write(int fd, const void* buffer, size_t count)
	{
		if (count == 0)
		{
			TRY(m_open_file_descriptors.inode_of(fd));
			return 0;
		}

		auto* buffer_region = TRY(validate_and_pin_pointer_access(buffer, count, false));
		BAN::ScopeGuard _([buffer_region] { buffer_region->unpin(); });
		return TRY(m_open_file_descriptors.write(fd, BAN::ConstByteSpan(static_cast<const uint8_t*>(buffer), count)));
	}

	BAN::ErrorOr<long> Process::sys_access(const char* user_path, int amode)
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

		char path[PATH_MAX];
		TRY(read_string_from_user(user_path, path, PATH_MAX));

		LockGuard _(m_process_lock);

		auto credentials = m_credentials;
		credentials.set_euid(credentials.ruid());
		credentials.set_egid(credentials.rgid());

		auto relative_parent = TRY(find_relative_parent(AT_FDCWD, path));
		TRY(VirtualFileSystem::get().file_from_relative_path(m_root_file.inode, relative_parent, credentials, path, flags));

		return 0;
	}

	BAN::ErrorOr<long> Process::sys_mkdirat(int fd, const char* user_path, mode_t mode)
	{
		char path[PATH_MAX];
		TRY(read_string_from_user(user_path, path, PATH_MAX));

		struct uid_gid_t { uid_t uid; gid_t gid; };
		const auto [uid, gid] = ({
			LockGuard _(m_process_lock);
			uid_gid_t { m_credentials.euid(), m_credentials.egid() };
		});

		auto [parent, file_name] = TRY(find_parent_file(fd, path, O_SEARCH));
		if (!parent.inode->find_inode(file_name).is_error())
			return BAN::Error::from_errno(EEXIST);
		if (!parent.inode->can_access(m_credentials, O_WRONLY))
			return BAN::Error::from_errno(EACCES);
		TRY(parent.inode->create_directory(file_name, (mode & 0777) | Inode::Mode::IFDIR, uid, gid));

		return 0;
	}

	BAN::ErrorOr<long> Process::sys_mkfifoat(int fd, const char* user_path, mode_t mode)
	{
		char path[PATH_MAX];
		TRY(read_string_from_user(user_path, path, PATH_MAX));

		struct uid_gid_t { uid_t uid; gid_t gid; };
		const auto [uid, gid] = ({
			LockGuard _(m_process_lock);
			uid_gid_t { m_credentials.euid(), m_credentials.egid() };
		});

		auto [parent, file_name] = TRY(find_parent_file(fd, path, O_WRONLY));
		TRY(parent.inode->create_file(file_name, (mode & 0777) | Inode::Mode::IFIFO, uid, gid));

		return 0;
	}

	BAN::ErrorOr<long> Process::sys_hardlinkat(int fd1, const char* user_path1, int fd2, const char* user_path2, int flag)
	{
		char path1[PATH_MAX];
		if (user_path1 != nullptr)
			TRY(read_string_from_user(user_path1, path1, PATH_MAX));

		char path2[PATH_MAX];
		if (user_path2 != nullptr)
			TRY(read_string_from_user(user_path2, path2, PATH_MAX));

		auto inode = TRY(find_file(fd1, user_path1 ? path1 : nullptr, flag)).inode;
		if (inode->mode().ifdir())
			return BAN::Error::from_errno(EISDIR);

		auto [parent, file_name] = TRY(find_parent_file(fd2, user_path2 ? path2 : nullptr, O_WRONLY));
		if (!parent.inode->mode().ifdir())
			return BAN::Error::from_errno(ENOTDIR);

		TRY(parent.inode->link_inode(file_name, inode));

		return 0;
	}

	BAN::ErrorOr<long> Process::sys_renameat(int oldfd, const char* user_path_old, int newfd, const char* user_path_new)
	{
		char path_old[PATH_MAX];
		if (user_path_old != nullptr)
			TRY(read_string_from_user(user_path_old, path_old, PATH_MAX));

		char path_new[PATH_MAX];
		if (user_path_new != nullptr)
			TRY(read_string_from_user(user_path_new, path_new, PATH_MAX));

		auto [old_parent, old_name] = TRY(find_parent_file(oldfd, user_path_old ? path_old : nullptr, O_WRONLY));
		if (!old_parent.inode->mode().ifdir())
			return BAN::Error::from_errno(ENOTDIR);

		auto [new_parent, new_name] = TRY(find_parent_file(newfd, user_path_new ? path_new : nullptr, O_WRONLY));
		if (!new_parent.inode->mode().ifdir())
			return BAN::Error::from_errno(ENOTDIR);

		TRY(new_parent.inode->rename_inode(old_parent.inode, old_name, new_name));

		return 0;
	}

	BAN::ErrorOr<long> Process::sys_unlinkat(int fd, const char* user_path, int flag)
	{
		if (flag && flag != AT_REMOVEDIR)
			return BAN::Error::from_errno(EINVAL);

		char path[PATH_MAX];
		if (user_path != nullptr)
			TRY(read_string_from_user(user_path, path, PATH_MAX));

		auto [parent, file_name] = TRY(find_parent_file(fd, user_path ? path : nullptr, O_WRONLY));

		const auto inode = TRY(parent.inode->find_inode(file_name));

		if (inode->mode().ifdir() != (flag == AT_REMOVEDIR))
			return BAN::Error::from_errno(flag ? EPERM : ENOTDIR);

		if (parent.inode->mode().mode & Inode::Mode::ISVTX)
		{
			LockGuard _(m_process_lock);
			if (m_credentials.ruid() != parent.inode->uid() && m_credentials.ruid() != inode->uid())
				return BAN::Error::from_errno(EPERM);
		}

		TRY(parent.inode->unlink(file_name));

		return 0;
	}

	BAN::ErrorOr<long> Process::sys_readlinkat(int fd, const char* user_path, char* buffer, size_t bufsize)
	{
		char path[PATH_MAX];
		if (user_path != nullptr)
			TRY(read_string_from_user(user_path, path, PATH_MAX));

		auto inode = TRY(find_file(fd, user_path ? path : nullptr, O_NOFOLLOW | O_RDONLY)).inode;

		auto link_target = TRY(inode->link_target());

		const size_t byte_count = BAN::Math::min<size_t>(link_target.size(), bufsize);
		TRY(write_to_user(buffer, link_target.data(), byte_count));

		return byte_count;
	}

	BAN::ErrorOr<long> Process::sys_symlinkat(const char* user_path1, int fd, const char* user_path2)
	{
		char path1[PATH_MAX];
		TRY(read_string_from_user(user_path1, path1, PATH_MAX));

		char path2[PATH_MAX];
		TRY(read_string_from_user(user_path2, path2, PATH_MAX));

		TRY(create_file(fd, path2, 0777 | Inode::Mode::IFLNK));

		auto symlink = TRY(find_file(fd, path2, O_NOFOLLOW));
		TRY(symlink.inode->set_link_target(path1));

		return 0;
	}

	BAN::ErrorOr<long> Process::sys_flock(int fd, int op)
	{
		auto maybe_error = m_open_file_descriptors.flock(fd, op);
		if (maybe_error.is_error() && maybe_error.error().get_error_code() == ENOMEM)
			return BAN::Error::from_errno(ENOLCK);
		if (maybe_error.is_error())
			return maybe_error.error();
		return 0;
	}

	BAN::ErrorOr<long> Process::sys_pread(int fd, void* buffer, size_t count, off_t offset)
	{
		auto inode = TRY(m_open_file_descriptors.inode_of(fd));

		auto* buffer_region = TRY(validate_and_pin_pointer_access(buffer, count, true));
		BAN::ScopeGuard _([buffer_region] { buffer_region->unpin(); });

		return TRY(inode->read(offset, { reinterpret_cast<uint8_t*>(buffer), count }));
	}

	BAN::ErrorOr<long> Process::sys_pwrite(int fd, const void* buffer, size_t count, off_t offset)
	{
		auto inode = TRY(m_open_file_descriptors.inode_of(fd));

		auto* buffer_region = TRY(validate_and_pin_pointer_access(buffer, count, false));
		BAN::ScopeGuard _([buffer_region] { buffer_region->unpin(); });

		return TRY(inode->write(offset, { reinterpret_cast<const uint8_t*>(buffer), count }));	}

	BAN::ErrorOr<long> Process::sys_fchmodat(int fd, const char* user_path, mode_t mode, int flag)
	{
		if (flag & ~AT_SYMLINK_NOFOLLOW)
			return BAN::Error::from_errno(EINVAL);
		if (flag == AT_SYMLINK_NOFOLLOW)
			flag = O_NOFOLLOW;

		char path[PATH_MAX];
		if (user_path != nullptr)
			TRY(read_string_from_user(user_path, path, PATH_MAX));

		auto inode = TRY(find_file(fd, user_path ? path : nullptr, flag)).inode;

		{
			LockGuard _(m_process_lock);
			if (!m_credentials.is_superuser() && inode->uid() != m_credentials.euid())
			{
				dwarnln("cannot chmod uid {} vs {}", inode->uid(), m_credentials.euid());
				return BAN::Error::from_errno(EPERM);
			}
		}

		TRY(inode->chmod(mode & ~S_IFMASK));

		return 0;
	}

	BAN::ErrorOr<long> Process::sys_fchownat(int fd, const char* user_path, uid_t uid, gid_t gid, int flag)
	{
		if (flag & ~AT_SYMLINK_NOFOLLOW)
			return BAN::Error::from_errno(EINVAL);
		if (flag == AT_SYMLINK_NOFOLLOW)
			flag = O_NOFOLLOW;

		char path[PATH_MAX];
		if (user_path != nullptr)
			TRY(read_string_from_user(user_path, path, PATH_MAX));

		auto inode = TRY(find_file(fd, user_path ? path : nullptr, flag)).inode;

		{
			LockGuard _(m_process_lock);
			if (uid != -1 && !m_credentials.is_superuser())
				return BAN::Error::from_errno(EPERM);
			if (gid != -1 && !m_credentials.is_superuser() && (m_credentials.euid() != uid || !m_credentials.has_egid(gid)))
				return BAN::Error::from_errno(EPERM);
		}

		if (uid == -1)
			uid = inode->uid();
		if (gid == -1)
			gid = inode->gid();
		TRY(inode->chown(uid, gid));

		return 0;
	}

	BAN::ErrorOr<long> Process::sys_utimensat(int fd, const char* user_path, const struct timespec user_times[2], int flag)
	{
		if (flag & ~AT_SYMLINK_NOFOLLOW)
			return BAN::Error::from_errno(EINVAL);
		if (flag == AT_SYMLINK_NOFOLLOW)
			flag = O_NOFOLLOW;

		const timespec current_ts = SystemTimer::get().real_time();

		timespec times[2];
		if (user_times == nullptr)
		{
			times[0] = current_ts;
			times[1] = current_ts;
		}
		else
		{
			TRY(read_from_user(user_times, times, 2 * sizeof(timespec)));

			for (size_t i = 0; i < 2; i++)
			{
				if (times[i].tv_nsec == UTIME_OMIT)
					;
				else if (times[i].tv_nsec == UTIME_NOW)
					times[i] = current_ts;
				else if (times[i].tv_nsec < 0 || times[i].tv_nsec >= 1'000'000'000)
					return BAN::Error::from_errno(EINVAL);
			}
		}

		if (times[0].tv_nsec == UTIME_OMIT && times[1].tv_nsec == UTIME_OMIT)
			return 0;

		char path[PATH_MAX];
		if (user_path != nullptr)
			TRY(read_string_from_user(user_path, path, PATH_MAX));

		auto inode = TRY(find_file(fd, user_path ? path : nullptr, flag)).inode;

		{
			LockGuard _(m_process_lock);
			if (!m_credentials.is_superuser() && inode->uid() != m_credentials.euid())
			{
				dwarnln("cannot chmod uid {} vs {}", inode->uid(), m_credentials.euid());
				return BAN::Error::from_errno(EPERM);
			}
		}

		if (times[0].tv_nsec == UTIME_OMIT)
			times[0] = inode->atime();
		if (times[1].tv_nsec == UTIME_OMIT)
			times[1] = inode->mtime();

		TRY(inode->utimens(times));

		return 0;
	}

	BAN::ErrorOr<long> Process::sys_socket(int domain, int type, int protocol)
	{
		return TRY(m_open_file_descriptors.socket(domain, type, protocol));
	}

	BAN::ErrorOr<long> Process::sys_socketpair(int domain, int type, int protocol, int user_socket_vector[2])
	{
		int socket_vector[2];
		TRY(m_open_file_descriptors.socketpair(domain, type, protocol, socket_vector));

		if (auto ret = write_to_user(user_socket_vector, socket_vector, 2 * sizeof(int)); ret.is_error())
		{
			MUST(m_open_file_descriptors.close(socket_vector[0]));
			MUST(m_open_file_descriptors.close(socket_vector[1]));
			return ret.release_error();
		}

		return 0;
	}

	BAN::ErrorOr<long> Process::sys_getsockname(int socket, sockaddr* user_address, socklen_t* user_address_len)
	{
		socklen_t address_len;
		TRY(read_from_user(user_address_len, &address_len, sizeof(socklen_t)));
		if (address_len > static_cast<socklen_t>(sizeof(sockaddr_storage)))
			address_len = sizeof(sockaddr_storage);

		sockaddr_storage address;
		TRY(read_from_user(user_address, &address, address_len));

		auto inode = TRY(m_open_file_descriptors.inode_of(socket));
		if (!inode->mode().ifsock())
			return BAN::Error::from_errno(ENOTSOCK);

		TRY(inode->getsockname(reinterpret_cast<sockaddr*>(&address), &address_len));

		TRY(write_to_user(user_address_len, &address_len, sizeof(socklen_t)));
		TRY(write_to_user(user_address, &address, address_len));

		return 0;
	}

	BAN::ErrorOr<long> Process::sys_getpeername(int socket, sockaddr* user_address, socklen_t* user_address_len)
	{
		socklen_t address_len;
		TRY(read_from_user(user_address_len, &address_len, sizeof(socklen_t)));
		if (address_len > static_cast<socklen_t>(sizeof(sockaddr_storage)))
			address_len = sizeof(sockaddr_storage);

		sockaddr_storage address;
		TRY(read_from_user(user_address, &address, address_len));

		auto inode = TRY(m_open_file_descriptors.inode_of(socket));
		if (!inode->mode().ifsock())
			return BAN::Error::from_errno(ENOTSOCK);

		TRY(inode->getpeername(reinterpret_cast<sockaddr*>(&address), &address_len));

		TRY(write_to_user(user_address_len, &address_len, sizeof(socklen_t)));
		TRY(write_to_user(user_address, &address, address_len));

		return 0;
	}

	BAN::ErrorOr<long> Process::sys_getsockopt(int socket, int level, int option_name, void* user_option_value, socklen_t* user_option_len)
	{
		socklen_t option_len;
		TRY(read_from_user(user_option_len, &option_len, sizeof(socklen_t)));

		if (option_len < 0)
			return BAN::Error::from_errno(EINVAL);

		auto inode = TRY(m_open_file_descriptors.inode_of(socket));
		if (!inode->mode().ifsock())
			return BAN::Error::from_errno(ENOTSOCK);

		auto* buffer = TRY(validate_and_pin_pointer_access(user_option_value, option_len, true));
		BAN::ScopeGuard _([buffer] { buffer->unpin(); });

		TRY(inode->getsockopt(level, option_name, user_option_value, &option_len));
		TRY(write_to_user(user_option_len, &option_len, sizeof(socklen_t)));

		return 0;
	}

	BAN::ErrorOr<long> Process::sys_setsockopt(int socket, int level, int option_name, const void* user_option_value, socklen_t option_len)
	{
		if (option_len < 0)
			return BAN::Error::from_errno(EINVAL);

		auto inode = TRY(m_open_file_descriptors.inode_of(socket));
		if (!inode->mode().ifsock())
			return BAN::Error::from_errno(ENOTSOCK);

		auto* buffer = TRY(validate_and_pin_pointer_access(user_option_value, option_len, false));
		BAN::ScopeGuard _([buffer] { buffer->unpin(); });

		TRY(inode->setsockopt(level, option_name, user_option_value, option_len));

		return 0;
	}

	BAN::ErrorOr<long> Process::sys_accept(int socket, sockaddr* address, socklen_t* address_len, int flags)
	{
		if (!address != !address_len)
			return BAN::Error::from_errno(EINVAL);
		if (flags & ~(SOCK_NONBLOCK | SOCK_CLOEXEC))
			return BAN::Error::from_errno(EINVAL);

		MemoryRegion* address_region1 = nullptr;
		MemoryRegion* address_region2 = nullptr;

		BAN::ScopeGuard _([&] {
			if (address_region1)
				address_region1->unpin();
			if (address_region2)
				address_region2->unpin();
		});

		if (address_len)
		{
			address_region1 = TRY(validate_and_pin_pointer_access(address_len, sizeof(address_len), true));
			address_region2 = TRY(validate_and_pin_pointer_access(address, *address_len, true));
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

	BAN::ErrorOr<long> Process::sys_bind(int socket, const sockaddr* user_address, socklen_t address_len)
	{
		if (address_len > static_cast<socklen_t>(sizeof(sockaddr_storage)))
			address_len = sizeof(sockaddr_storage);

		sockaddr_storage address;
		TRY(read_from_user(user_address, &address, address_len));

		auto inode = TRY(m_open_file_descriptors.inode_of(socket));
		if (!inode->mode().ifsock())
			return BAN::Error::from_errno(ENOTSOCK);

		TRY(inode->bind(reinterpret_cast<sockaddr*>(&address), address_len));

		return 0;
	}

	BAN::ErrorOr<long> Process::sys_connect(int socket, const sockaddr* user_address, socklen_t address_len)
	{
		if (address_len > static_cast<socklen_t>(sizeof(sockaddr_storage)))
			address_len = sizeof(sockaddr_storage);

		sockaddr_storage address;
		TRY(read_from_user(user_address, &address, address_len));

		auto inode = TRY(m_open_file_descriptors.inode_of(socket));
		if (!inode->mode().ifsock())
			return BAN::Error::from_errno(ENOTSOCK);

		TRY(inode->connect(reinterpret_cast<sockaddr*>(&address), address_len));

		return 0;
	}

	BAN::ErrorOr<long> Process::sys_listen(int socket, int backlog)
	{
		auto inode = TRY(m_open_file_descriptors.inode_of(socket));
		if (!inode->mode().ifsock())
			return BAN::Error::from_errno(ENOTSOCK);

		TRY(inode->listen(backlog));

		return 0;
	}

	BAN::ErrorOr<long> Process::sys_recvmsg(int socket, msghdr* user_message, int flags)
	{
		msghdr message;
		TRY(read_from_user(user_message, &message, sizeof(msghdr)));

		BAN::Vector<MemoryRegion*> regions;
		TRY(regions.reserve(!!message.msg_name + !!message.msg_control + !!message.msg_iov));

		BAN::ScopeGuard _([&regions] {
			for (auto* region : regions)
				region->unpin();
		});

		if (message.msg_name)
			TRY(regions.push_back(TRY(validate_and_pin_pointer_access(message.msg_name, message.msg_namelen, true))));
		if (message.msg_control)
			TRY(regions.push_back(TRY(validate_and_pin_pointer_access(message.msg_control, message.msg_controllen, true))));
		if (message.msg_iov)
		{
			TRY(regions.push_back(TRY(validate_and_pin_pointer_access(message.msg_iov, message.msg_iovlen * sizeof(iovec), true))));
			TRY(regions.reserve(regions.size() + message.msg_iovlen));
			for (int i = 0; i < message.msg_iovlen; i++)
				TRY(regions.push_back(TRY(validate_and_pin_pointer_access(message.msg_iov[i].iov_base, message.msg_iov[i].iov_len, true))));
		}

		const auto ret = TRY(m_open_file_descriptors.recvmsg(socket, message, flags));

		TRY(write_to_user(user_message, &message, sizeof(msghdr)));

		return ret;
	}

	BAN::ErrorOr<long> Process::sys_sendmsg(int socket, const msghdr* user_message, int flags)
	{
		msghdr message;
		TRY(read_from_user(user_message, &message, sizeof(msghdr)));

		BAN::Vector<MemoryRegion*> regions;
		TRY(regions.reserve(!!message.msg_name + !!message.msg_control + !!message.msg_iov));

		BAN::ScopeGuard _([&regions] {
			for (auto* region : regions)
				region->unpin();
		});

		if (message.msg_name)
			TRY(regions.push_back(TRY(validate_and_pin_pointer_access(message.msg_name, message.msg_namelen, false))));
		if (message.msg_control)
			TRY(regions.push_back(TRY(validate_and_pin_pointer_access(message.msg_control, message.msg_controllen, false))));
		if (message.msg_iov)
		{
			TRY(regions.push_back(TRY(validate_and_pin_pointer_access(message.msg_iov, message.msg_iovlen * sizeof(iovec), false))));
			TRY(regions.reserve(regions.size() + message.msg_iovlen));
			for (int i = 0; i < message.msg_iovlen; i++)
				TRY(regions.push_back(TRY(validate_and_pin_pointer_access(message.msg_iov[i].iov_base, message.msg_iov[i].iov_len, false))));
		}

		return TRY(m_open_file_descriptors.sendmsg(socket, message, flags));
	}

	BAN::ErrorOr<long> Process::sys_ioctl(int fildes, unsigned long request, void* arg)
	{
		return m_open_file_descriptors.ioctl(fildes, request, arg);
	}

	BAN::ErrorOr<long> Process::sys_pselect(sys_pselect_t* user_arguments)
	{
		sys_pselect_t arguments;
		TRY(read_from_user(user_arguments, &arguments, sizeof(sys_pselect_t)));

		const int max_nfds = BAN::Math::min(m_open_file_descriptors.get_max_open_fd(), FD_SETSIZE) + 1;
		if (arguments.nfds > max_nfds)
			arguments.nfds = max_nfds;

		const auto init_fd_set = [](fd_set* user, fd_set* target) -> BAN::ErrorOr<void> {
			if (user == nullptr)
				FD_ZERO(target);
			else
				TRY(read_from_user(user, target, sizeof(fd_set)));
			return {};
		};

		fd_set rfds, wfds, efds;
		TRY(init_fd_set(arguments.readfds,  &rfds));
		TRY(init_fd_set(arguments.writefds, &wfds));
		TRY(init_fd_set(arguments.errorfds, &efds));

		const auto old_sigmask = Thread::current().m_signal_block_mask;
		if (arguments.sigmask)
		{
			sigset_t sigmask;
			TRY(read_from_user(user_arguments->sigmask, &sigmask, sizeof(sigset_t)));
			Thread::current().m_signal_block_mask = sigmask;
		}
		BAN::ScopeGuard sigmask_restore([old_sigmask] { Thread::current().m_signal_block_mask = old_sigmask; });

		uint64_t waketime_ns = BAN::numeric_limits<uint64_t>::max();
		if (arguments.timeout)
		{
			timespec timeout;
			TRY(read_from_user(arguments.timeout, &timeout, sizeof(timespec)));
			waketime_ns =
				SystemTimer::get().ns_since_boot() +
				timeout.tv_sec * 1'000'000'000 +
				timeout.tv_nsec;
		}

		fd_set ret_rfds, ret_wfds, ret_efds;
		FD_ZERO(&ret_rfds);
		FD_ZERO(&ret_wfds);
		FD_ZERO(&ret_efds);

		{

			size_t return_value = 0;
			for (int fd = 0; fd < arguments.nfds; fd++)
			{
				const bool wantr = FD_ISSET(fd, &rfds);
				const bool wantw = FD_ISSET(fd, &wfds);
				const bool wante = FD_ISSET(fd, &efds);
				if (!wantr && !wantw && !wante)
					continue;

				auto inode = TRY(m_open_file_descriptors.inode_of(fd));
				if (wantr && inode->can_read())
					{ FD_SET(fd, &ret_rfds); return_value++; }
				if (wantw && inode->can_write())
					{ FD_SET(fd, &ret_wfds); return_value++; }
				if (wante && inode->has_error())
					{ FD_SET(fd, &ret_efds); return_value++; }
			}

			if (return_value || SystemTimer::get().ns_since_boot() >= waketime_ns)
			{
				if (arguments.readfds)
					TRY(write_to_user(arguments.readfds,  &ret_rfds, sizeof(fd_set)));
				if (arguments.writefds)
					TRY(write_to_user(arguments.writefds, &ret_wfds, sizeof(fd_set)));
				if (arguments.errorfds)
					TRY(write_to_user(arguments.errorfds, &ret_efds, sizeof(fd_set)));
				return return_value;
			}
		}

		auto epoll = TRY(Epoll::create());
		for (int fd = 0; fd < arguments.nfds; fd++)
		{
			uint32_t events = 0;
			if (FD_ISSET(fd, &rfds))
				events |= EPOLLIN;
			if (FD_ISSET(fd, &wfds))
				events |= EPOLLOUT;
			if (FD_ISSET(fd, &efds))
				events |= EPOLLERR;
			if (events == 0)
				continue;

			auto inode = TRY(m_open_file_descriptors.inode_of(fd));
			TRY(epoll->ctl(EPOLL_CTL_ADD, fd, inode, { .events = events, .data = { .fd = fd }}));
		}

		BAN::Vector<epoll_event> event_buffer;
		TRY(event_buffer.resize(arguments.nfds));

		const size_t waited_events = TRY(epoll->wait(event_buffer.span(), waketime_ns));

		size_t return_value = 0;
		for (size_t i = 0; i < waited_events; i++)
		{
			const int fd = event_buffer[i].data.fd;
			if (FD_ISSET(fd, &rfds) && event_buffer[i].events & (EPOLLIN | EPOLLHUP))
				{ FD_SET(fd, &ret_rfds); return_value++; }
			if (FD_ISSET(fd, &wfds) && event_buffer[i].events & (EPOLLOUT))
				{ FD_SET(fd, &ret_wfds); return_value++; }
			if (FD_ISSET(fd, &efds) && event_buffer[i].events & (EPOLLERR))
				{ FD_SET(fd, &ret_efds); return_value++; }
		}

		if (arguments.readfds)
			TRY(write_to_user(arguments.readfds,  &ret_rfds, sizeof(fd_set)));
		if (arguments.writefds)
			TRY(write_to_user(arguments.writefds, &ret_wfds, sizeof(fd_set)));
		if (arguments.errorfds)
			TRY(write_to_user(arguments.errorfds, &ret_efds, sizeof(fd_set)));
		return return_value;
	}

	BAN::ErrorOr<long> Process::sys_ppoll(pollfd* fds, nfds_t nfds, const timespec* user_timeout, const sigset_t* user_sigmask)
	{
		auto* fds_region = TRY(validate_and_pin_pointer_access(fds, nfds * sizeof(pollfd), true));
		BAN::ScopeGuard _([fds_region] { fds_region->unpin(); });

		const auto old_sigmask = Thread::current().m_signal_block_mask;
		if (user_sigmask != nullptr)
		{
			sigset_t sigmask;
			TRY(read_from_user(user_sigmask, &sigmask, sizeof(sigmask)));
			Thread::current().m_signal_block_mask = sigmask;
		}
		BAN::ScopeGuard sigmask_restore([old_sigmask] { Thread::current().m_signal_block_mask = old_sigmask; });

		uint64_t waketime_ns = BAN::numeric_limits<uint64_t>::max();
		if (user_timeout != nullptr)
		{
			timespec timeout;
			TRY(read_from_user(user_timeout, &timeout, sizeof(timespec)));
			waketime_ns =
				SystemTimer::get().ns_since_boot() +
				timeout.tv_sec * 1'000'000'000 +
				timeout.tv_nsec;
		}

		size_t return_value = 0;

		for (nfds_t i = 0; i < nfds; i++)
		{
			fds[i].revents = 0;

			if (fds[i].fd < 0)
				continue;
			auto inode_or_error = m_open_file_descriptors.inode_of(fds[i].fd);
			if (inode_or_error.is_error())
			{
				fds[i].revents |= POLLNVAL;
				return_value++;
				continue;
			}

			auto inode = inode_or_error.release_value();

			if (inode->has_hungup())
				fds[i].revents |= POLLHUP;
			if (inode->has_error())
				fds[i].revents |= POLLERR;
			if ((fds[i].events & (POLLIN  | POLLRDNORM)) && inode->can_read())
				fds[i].revents |= fds[i].events & (POLLIN  | POLLRDNORM);
			if ((fds[i].events & (POLLOUT | POLLWRNORM)) && inode->can_write())
				fds[i].revents |= fds[i].events & (POLLOUT | POLLWRNORM);
			// POLLPRI
			// POLLRDBAND
			// POLLWRBAND

			if (fds[i].revents)
				return_value++;
		}

		if (return_value || SystemTimer::get().ns_since_boot() >= waketime_ns)
			return return_value;

		uint32_t events_per_fd[OPEN_MAX] {};
		for (nfds_t i = 0; i < nfds; i++)
			if (fds[i].fd >= 0 && fds[i].fd < OPEN_MAX)
				events_per_fd[fds[i].fd] |= fds[i].events;

		size_t fd_count = 0;

		auto epoll = TRY(Epoll::create());
		for (int fd = 0; fd < OPEN_MAX; fd++)
		{
			if (events_per_fd[fd] == 0)
				continue;

			auto inode = TRY(m_open_file_descriptors.inode_of(fd));

			uint32_t events = 0;
			if (events_per_fd[fd] & (POLLIN  | POLLRDNORM))
				events |= EPOLLIN;
			if (events_per_fd[fd] & (POLLOUT | POLLWRNORM))
				events |= EPOLLOUT;
			if (events_per_fd[fd] & POLLPRI)
				events |= EPOLLPRI;
			// POLLRDBAND
			// POLLWRBAND

			TRY(epoll->ctl(EPOLL_CTL_ADD, fd, inode, { .events = events, .data = { .fd = fd }}));

			fd_count++;
		}

		BAN::Vector<epoll_event> event_buffer;
		TRY(event_buffer.resize(fd_count));

		const size_t waited_events = TRY(epoll->wait(event_buffer.span(), waketime_ns));

		for (size_t i = 0; i < nfds; i++)
		{
			if (fds[i].fd < 0)
				continue;

			for (size_t j = 0; j < waited_events; j++)
			{
				if (fds[i].fd != event_buffer[j].data.fd)
					continue;
				const uint32_t wanted = fds[i].events;
				const uint32_t got = event_buffer[j].events;
				if (got & EPOLLIN)
					fds[i].revents |= wanted & (POLLIN  | POLLRDNORM);
				if (got & EPOLLOUT)
					fds[i].revents |= wanted & (POLLOUT | POLLWRNORM);
				if (got & EPOLLPRI)
					fds[i].revents |= wanted & POLLPRI;
				if (got & EPOLLERR)
					fds[i].revents |= POLLERR;
				if (got & EPOLLHUP)
					fds[i].revents |= POLLHUP;
				// POLLRDBAND
				// POLLWRBAND
				if (fds[i].revents)
					return_value++;
				break;
			}
		}

		return return_value;
	}

	BAN::ErrorOr<long> Process::sys_epoll_create1(int flags)
	{
		if (flags && (flags & ~EPOLL_CLOEXEC))
			return BAN::Error::from_errno(EINVAL);
		if (flags & EPOLL_CLOEXEC)
			flags = O_CLOEXEC;

		VirtualFileSystem::File epoll_file;
		epoll_file.inode = TRY(Epoll::create());
		TRY(epoll_file.canonical_path.append("<epoll>"_sv));

		return TRY(m_open_file_descriptors.open(BAN::move(epoll_file), flags | O_RDWR));
	}

	BAN::ErrorOr<long> Process::sys_epoll_ctl(int epfd, int op, int fd, epoll_event* user_event)
	{
		if (epfd == fd)
			return BAN::Error::from_errno(EINVAL);
		if (op != EPOLL_CTL_DEL && user_event == nullptr)
			return BAN::Error::from_errno(EINVAL);

		auto epoll_inode = TRY(m_open_file_descriptors.inode_of(epfd));
		if (!epoll_inode->is_epoll())
			return BAN::Error::from_errno(EINVAL);

		auto inode = TRY(m_open_file_descriptors.inode_of(fd));

		epoll_event event {};
		if (user_event)
			TRY(read_from_user(user_event, &event, sizeof(epoll_event)));

		TRY(static_cast<Epoll*>(epoll_inode.ptr())->ctl(op, fd, inode, event));

		return 0;
	}

	BAN::ErrorOr<long> Process::sys_epoll_pwait2(int epfd, epoll_event* events, int maxevents, const timespec* user_timeout, const sigset_t* user_sigmask)
	{
		if (maxevents <= 0)
			return BAN::Error::from_errno(EINVAL);

		auto epoll_inode = TRY(m_open_file_descriptors.inode_of(epfd));
		if (!epoll_inode->is_epoll())
			return BAN::Error::from_errno(EINVAL);

		uint64_t waketime_ns = BAN::numeric_limits<uint64_t>::max();
		if (user_timeout != nullptr)
		{
			timespec timeout;
			TRY(read_from_user(user_timeout, &timeout, sizeof(timespec)));
			waketime_ns =
				SystemTimer::get().ns_since_boot() +
				timeout.tv_sec * 1'000'000'000 +
				timeout.tv_nsec;
		}

		auto* events_region = TRY(validate_and_pin_pointer_access(events, maxevents * sizeof(epoll_event), true));
		BAN::ScopeGuard _([events_region] { events_region->unpin(); });

		const auto old_sigmask = Thread::current().m_signal_block_mask;
		if (user_sigmask)
		{
			sigset_t sigmask;
			TRY(read_from_user(user_sigmask, &sigmask, sizeof(sigset_t)));
			Thread::current().m_signal_block_mask = sigmask;
		}
		BAN::ScopeGuard sigmask_restore([old_sigmask] { Thread::current().m_signal_block_mask = old_sigmask; });

		return TRY(static_cast<Epoll*>(epoll_inode.ptr())->wait(BAN::Span<epoll_event>(events, maxevents), waketime_ns));
	}

	BAN::ErrorOr<long> Process::sys_eventfd(unsigned int initval, int flags)
	{
		if (flags & ~(EFD_CLOEXEC | EFD_NONBLOCK | EFD_SEMAPHORE))
			return BAN::Error::from_errno(EINVAL);

		int oflags = 0;
		if (flags & EFD_CLOEXEC)
			oflags |= O_CLOEXEC;
		if (flags & EFD_NONBLOCK)
			oflags |= O_NONBLOCK;

		const bool is_semaphore = !!(flags & EFD_SEMAPHORE);

		return TRY(m_open_file_descriptors.open(
			VirtualFileSystem::File {
				TRY(EventFD::create(initval, is_semaphore)),
				"<eventfd>"_sv
			}, oflags
		));
	}

	BAN::ErrorOr<long> Process::sys_pipe(int user_fildes[2])
	{
		int fildes[2];
		TRY(m_open_file_descriptors.pipe(fildes));

		if (auto ret = write_to_user(user_fildes, fildes, 2 * sizeof(int)); ret.is_error())
		{
			MUST(m_open_file_descriptors.close(fildes[0]));
			MUST(m_open_file_descriptors.close(fildes[1]));
			return ret.release_error();
		}

		return 0;
	}

	BAN::ErrorOr<long> Process::sys_dup2(int fildes, int fildes2)
	{
		return TRY(m_open_file_descriptors.dup2(fildes, fildes2));
	}

	BAN::ErrorOr<long> Process::sys_fcntl(int fildes, int cmd, uintptr_t extra)
	{
		return TRY(m_open_file_descriptors.fcntl(fildes, cmd, extra));
	}

	BAN::ErrorOr<long> Process::sys_seek(int fd, off_t offset, int whence)
	{
		return TRY(m_open_file_descriptors.seek(fd, offset, whence));
	}

	BAN::ErrorOr<long> Process::sys_tell(int fd)
	{
		return TRY(m_open_file_descriptors.tell(fd));
	}

	BAN::ErrorOr<long> Process::sys_ftruncate(int fd, off_t length)
	{
		TRY(m_open_file_descriptors.truncate(fd, length));
		return 0;
	}

	BAN::ErrorOr<long> Process::sys_fsync(int fd)
	{
		auto inode = TRY(m_open_file_descriptors.inode_of(fd));
		TRY(inode->fsync());
		return 0;
	}

	BAN::ErrorOr<long> Process::sys_fstatat(int fd, const char* user_path, struct stat* user_buf, int flag)
	{
		if (flag & ~AT_SYMLINK_NOFOLLOW)
			return BAN::Error::from_errno(EINVAL);
		if (flag == AT_SYMLINK_NOFOLLOW)
			flag = O_NOFOLLOW;

		char path[PATH_MAX];
		if (user_path != nullptr)
			TRY(read_string_from_user(user_path, path, PATH_MAX));

		auto inode = TRY(find_file(fd, user_path ? path : nullptr, flag)).inode;

		const struct stat buf {
			.st_dev     = inode->dev(),
			.st_ino     = inode->ino(),
			.st_mode    = inode->mode().mode,
			.st_nlink   = inode->nlink(),
			.st_uid     = inode->uid(),
			.st_gid     = inode->gid(),
			.st_rdev    = inode->rdev(),
			.st_size    = inode->size(),
			.st_atim    = inode->atime(),
			.st_mtim    = inode->mtime(),
			.st_ctim    = inode->ctime(),
			.st_blksize = inode->blksize(),
			.st_blocks  = inode->blocks(),
		};

		TRY(write_to_user(user_buf, &buf, sizeof(struct stat)));

		return 0;
	}

	BAN::ErrorOr<long> Process::sys_fstatvfsat(int fd, const char* user_path, struct statvfs* user_buf)
	{
		char path[PATH_MAX];
		if (user_path != nullptr)
			TRY(read_string_from_user(user_path, path, PATH_MAX));

		auto inode = TRY(find_file(fd, user_path ? path : nullptr, 0)).inode;

		const auto* fs = inode->filesystem();
		if (fs == nullptr)
		{
			ASSERT(inode->mode().ifsock() || inode->mode().ififo());
			dwarnln("TODO: fstatvfs on sockets or pipe?");
			return BAN::Error::from_errno(EINVAL);
		}

		const struct statvfs buf {
			.f_bsize   = fs->bsize(),
			.f_frsize  = fs->frsize(),
			.f_blocks  = fs->blocks(),
			.f_bfree   = fs->bfree(),
			.f_bavail  = fs->bavail(),
			.f_files   = fs->files(),
			.f_ffree   = fs->ffree(),
			.f_favail  = fs->favail(),
			.f_fsid    = fs->fsid(),
			.f_flag    = fs->flag(),
			.f_namemax = fs->namemax(),
		};

		TRY(write_to_user(user_buf, &buf, sizeof(struct statvfs)));

		return 0;
	}

	BAN::ErrorOr<long> Process::sys_realpath(const char* user_path, char* user_buffer)
	{
		char path[PATH_MAX];
		TRY(read_string_from_user(user_path, path, PATH_MAX));

		auto file = TRY(find_file(AT_FDCWD, path, O_RDONLY));
		if (file.canonical_path.size() >= PATH_MAX)
			return BAN::Error::from_errno(ENAMETOOLONG);

		TRY(write_to_user(user_buffer, file.canonical_path.data(), file.canonical_path.size() + 1));

		return file.canonical_path.size();
	}

	BAN::ErrorOr<long> Process::sys_sync(bool should_block)
	{
		DevFileSystem::get().initiate_sync(should_block);
		return 0;
	}

	BAN::ErrorOr<long> Process::sys_get_nprocessor()
	{
		return Processor::count();
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
		if (BAN::Math::will_multiplication_overflow(list_len, sizeof(struct dirent)))
			return BAN::Error::from_errno(EOVERFLOW);

		auto* list_region = TRY(validate_and_pin_pointer_access(list, list_len * sizeof(struct dirent), true));
		BAN::ScopeGuard _([list_region] { list_region->unpin(); });
		return TRY(m_open_file_descriptors.read_dir_entries(fd, list, list_len));
	}

	BAN::ErrorOr<long> Process::sys_getcwd(char* user_buffer, size_t size)
	{
		if (size < m_working_directory.canonical_path.size() + 1)
			return BAN::Error::from_errno(ERANGE);
		TRY(write_to_user(user_buffer, m_working_directory.canonical_path.data(), m_working_directory.canonical_path.size() + 1));
		return reinterpret_cast<long>(user_buffer);
	}

	BAN::ErrorOr<long> Process::sys_chdir(const char* user_path)
	{
		char path[PATH_MAX];
		TRY(read_string_from_user(user_path, path, PATH_MAX));

		auto new_cwd = TRY(find_file(AT_FDCWD, path, O_SEARCH));
		if (!new_cwd.inode->mode().ifdir())
			return BAN::Error::from_errno(ENOTDIR);

		LockGuard _(m_process_lock);
		m_working_directory = BAN::move(new_cwd);
		return 0;
	}

	BAN::ErrorOr<long> Process::sys_fchdir(int fildes)
	{
		LockGuard _(m_process_lock);
		m_working_directory = TRY(m_open_file_descriptors.file_of(fildes));
		return 0;
	}

	BAN::ErrorOr<long> Process::sys_chroot(const char* user_path)
	{
		char path[PATH_MAX];
		TRY(read_string_from_user(user_path, path, PATH_MAX));

		auto new_root = TRY(find_file(AT_FDCWD, path, O_SEARCH));

		LockGuard _(m_process_lock);
		if (!m_credentials.is_superuser())
			return BAN::Error::from_errno(EACCES);
		m_root_file = BAN::move(new_root);
		return 0;
	}

	BAN::ErrorOr<BAN::Vector<BAN::UniqPtr<MemoryRegion>>> Process::split_memory_region(BAN::UniqPtr<MemoryRegion>&& region, vaddr_t base, size_t length)
	{
		ASSERT(base % PAGE_SIZE == 0);
		ASSERT(base < region->vaddr() + region->size());

		if (auto rem = length % PAGE_SIZE)
			length += PAGE_SIZE - rem;
		if (base + length > region->vaddr() + region->size())
			length = region->vaddr() + region->size() - base;

		BAN::Vector<BAN::UniqPtr<MemoryRegion>> result;
		TRY(result.reserve(3));

		if (region->vaddr() < base)
		{
			auto temp = TRY(region->split(base - region->vaddr()));
			MUST(result.push_back(BAN::move(region)));
			region = BAN::move(temp);
		}

		if (base + length < region->vaddr() + region->size())
		{
			auto temp = TRY(region->split(base + length - region->vaddr()));
			MUST(result.push_back(BAN::move(region)));
			region = BAN::move(temp);
		}

		MUST(result.push_back(BAN::move(region)));

		return result;
	}

	BAN::ErrorOr<long> Process::sys_mmap(const sys_mmap_t* user_args)
	{
		sys_mmap_t args;
		TRY(read_from_user(user_args, &args, sizeof(sys_mmap_t)));

		if (args.prot != PROT_NONE && (args.prot & ~(PROT_READ | PROT_WRITE | PROT_EXEC)))
			return BAN::Error::from_errno(EINVAL);

		if (!(args.flags & MAP_ANONYMOUS) && (args.off % PAGE_SIZE))
			return BAN::Error::from_errno(EINVAL);

		if (!(args.flags & MAP_PRIVATE) == !(args.flags & MAP_SHARED))
			return BAN::Error::from_errno(EINVAL);
		auto region_type = (args.flags & MAP_PRIVATE) ? MemoryRegion::Type::PRIVATE : MemoryRegion::Type::SHARED;

		PageTable::flags_t page_flags = 0;
		if (args.prot & PROT_READ)
			page_flags |= PageTable::Flags::Present;
		if (args.prot & PROT_WRITE)
			page_flags |= PageTable::Flags::ReadWrite | PageTable::Flags::Present;
		if (args.prot & PROT_EXEC)
			page_flags |= PageTable::Flags::Execute | PageTable::Flags::Present;

		if (page_flags == 0)
			page_flags = PageTable::Flags::Reserved;
		else
			page_flags |= PageTable::Flags::UserSupervisor;

		RWLockWRGuard _(m_memory_region_lock);

		AddressRange address_range;
		if (args.flags & (MAP_FIXED | MAP_FIXED_NOREPLACE))
		{
			const vaddr_t vaddr = reinterpret_cast<vaddr_t>(args.addr);
			if (vaddr == 0 || vaddr % PAGE_SIZE)
				return BAN::Error::from_errno(EINVAL);
			if (!PageTable::is_valid_pointer(vaddr))
				return BAN::Error::from_errno(ENOMEM);
			if (!PageTable::is_valid_pointer(vaddr + args.len))
				return BAN::Error::from_errno(ENOMEM);
			address_range = {
				.start = vaddr,
				.end = vaddr + args.len,
			};

			if (args.flags & MAP_FIXED_NOREPLACE)
				;
			else
			{
				const size_t first_index = find_mapped_region(vaddr);
				for (size_t i = first_index; i < m_mapped_regions.size(); i++)
				{
					if (!m_mapped_regions[i]->overlaps(vaddr, args.len))
						break;

					m_mapped_regions[i]->wait_not_pinned();
					auto temp = BAN::move(m_mapped_regions[i]);
					m_mapped_regions.remove(i--);

					if (temp->is_contained_by(vaddr, args.len))
						continue;

					auto new_regions = TRY(split_memory_region(BAN::move(temp), vaddr, args.len));
					for (auto& new_region : new_regions)
						if (!new_region->overlaps(vaddr, args.len))
							TRY(m_mapped_regions.insert(++i, BAN::move(new_region)));
				}
			}
		}
		else
		{
			bool use_address_hint = false;

			const vaddr_t vaddr = reinterpret_cast<vaddr_t>(args.addr);
			if (vaddr == 0 || vaddr % PAGE_SIZE)
				;
			else if (!PageTable::is_valid_pointer(vaddr))
				;
			else if (!PageTable::is_valid_pointer(vaddr + args.len))
				;
			else if (!page_table().is_range_free(vaddr, args.len))
				;
			else
			{
				use_address_hint = true;
				address_range = {
					.start = vaddr,
					.end = vaddr + args.len,
				};
			}

			if (!use_address_hint)
				address_range = TRY(find_free_address_range(args.len));
		}

		if (args.flags & MAP_ANONYMOUS)
		{
			auto region = TRY(MemoryBackedRegion::create(
				page_table(),
				args.len,
				address_range,
				region_type, page_flags,
				O_EXEC | O_RDWR
			));

			const vaddr_t region_vaddr = region->vaddr();
			TRY(add_mapped_region(BAN::move(region)));
			return region_vaddr;
		}

		auto inode = TRY(m_open_file_descriptors.inode_of(args.fildes));

		const auto status_flags = TRY(m_open_file_descriptors.status_flags_of(args.fildes));
		if (!(status_flags & O_RDONLY))
			return BAN::Error::from_errno(EACCES);
		if (region_type == MemoryRegion::Type::SHARED)
			if ((args.prot & PROT_WRITE) && !(status_flags & O_WRONLY))
				return BAN::Error::from_errno(EACCES);

		BAN::UniqPtr<MemoryRegion> region;
		if (inode->mode().ifreg())
		{
			region = TRY(FileBackedRegion::create(
				inode,
				page_table(),
				args.off, args.len,
				address_range,
				region_type, page_flags,
				status_flags
			));
		}
		else if (inode->is_device())
		{
			region = TRY(static_cast<Device&>(*inode).mmap_region(
				page_table(),
				args.off, args.len,
				address_range,
				region_type, page_flags,
				status_flags
			));
		}

		if (!region)
			return BAN::Error::from_errno(ENODEV);

		const vaddr_t region_vaddr = region->vaddr();
		TRY(add_mapped_region(BAN::move(region)));
		return region_vaddr;
	}

	BAN::ErrorOr<long> Process::sys_munmap(void* addr, size_t len)
	{
		if (len == 0)
			return BAN::Error::from_errno(EINVAL);

		vaddr_t vaddr = reinterpret_cast<vaddr_t>(addr);
		if (auto rem = vaddr % PAGE_SIZE)
		{
			vaddr -= rem;
			len += rem;
		}

		if (auto rem = len % PAGE_SIZE)
			len += PAGE_SIZE - rem;

		RWLockWRGuard _(m_memory_region_lock);

		const size_t first_index = find_mapped_region(vaddr);
		for (size_t i = first_index; i < m_mapped_regions.size(); i++)
		{
			if (!m_mapped_regions[i]->overlaps(vaddr, len))
				break;

			m_mapped_regions[i]->wait_not_pinned();
			auto temp = BAN::move(m_mapped_regions[i]);
			m_mapped_regions.remove(i--);

			if (temp->is_contained_by(vaddr, len))
				continue;

			auto new_regions = TRY(split_memory_region(BAN::move(temp), vaddr, len));
			for (auto& new_region : new_regions)
				if (!new_region->overlaps(vaddr, len))
					TRY(m_mapped_regions.insert(++i, BAN::move(new_region)));
		}

		return 0;
	}

	BAN::ErrorOr<long> Process::sys_mprotect(void* addr, size_t len, int prot)
	{
		if (len == 0)
			return BAN::Error::from_errno(EINVAL);

		const vaddr_t vaddr = reinterpret_cast<vaddr_t>(addr);
		if (vaddr % PAGE_SIZE != 0)
			return BAN::Error::from_errno(EINVAL);

		if (auto rem = len % PAGE_SIZE)
			len += PAGE_SIZE - rem;

		PageTable::flags_t flags = 0;
		if (prot & PROT_READ)
			flags |= PageTable::Flags::Present;
		if (prot & PROT_WRITE)
			flags |= PageTable::Flags::Present | PageTable::Flags::ReadWrite;
		if (prot & PROT_EXEC)
			flags |= PageTable::Flags::Present | PageTable::Flags::Execute;

		if (flags == 0)
			flags = PageTable::Flags::Reserved;
		else
			flags |= PageTable::Flags::UserSupervisor;

		RWLockWRGuard _(m_memory_region_lock);

		const size_t first_index = find_mapped_region(vaddr);
		for (size_t i = first_index; i < m_mapped_regions.size(); i++)
		{
			if (!m_mapped_regions[i]->overlaps(vaddr, len))
				break;

			if (!m_mapped_regions[i]->is_contained_by(vaddr, len))
			{
				m_mapped_regions[i]->wait_not_pinned();
				auto temp = BAN::move(m_mapped_regions[i]);
				m_mapped_regions.remove(i--);

				auto new_regions = TRY(split_memory_region(BAN::move(temp), vaddr, len));
				for (size_t j = 0; j < new_regions.size(); j++)
					TRY(m_mapped_regions.insert(i + j + 1, BAN::move(new_regions[j])));

				while (i + 1 < m_mapped_regions.size() && !m_mapped_regions[i + 1]->overlaps(vaddr, len))
					i++;

				continue;
			}

			auto& region = m_mapped_regions[i];
			const bool is_shared   = (region->type() == MemoryRegion::Type::SHARED);
			const bool is_writable = (region->status_flags() & O_WRONLY);
			const bool want_write  = (prot & PROT_WRITE);
			if (is_shared && want_write && !is_writable)
				return BAN::Error::from_errno(EACCES);

			// NOTE: don't change protection of regions in use
			region->wait_not_pinned();
			TRY(region->mprotect(flags));
		}

		return 0;
	}

	BAN::ErrorOr<long> Process::sys_msync(void* addr, size_t len, int flags)
	{
		if (flags != MS_SYNC && flags != MS_ASYNC && flags != MS_INVALIDATE)
			return BAN::Error::from_errno(EINVAL);

		RWLockRDGuard _(m_memory_region_lock);

		const vaddr_t vaddr = reinterpret_cast<vaddr_t>(addr);

		const size_t first_index = find_mapped_region(vaddr);
		for (size_t i = first_index; i < m_mapped_regions.size(); i++)
		{
			auto& region = *m_mapped_regions[i];
			if (vaddr >= region.vaddr() + region.size())
				break;
			if (region.overlaps(vaddr, len))
				TRY(region.msync(vaddr, len, flags));
		}

		return 0;
	}

	BAN::ErrorOr<long> Process::sys_shmget(key_t key, size_t size, int shmflg)
	{
		return TRY(SharedMemoryObjectManager::get().shmget(key, size, shmflg));
	}

	BAN::ErrorOr<long> Process::sys_shmctl(int shmid, int cmd, struct shmid_ds* user_buf)
	{
		TRY(SharedMemoryObjectManager::get().shmctl(shmid, cmd, user_buf));
		return 0;
	}

	BAN::ErrorOr<long> Process::sys_shmat(int shmid, const void* shmaddr, int shmflg)
	{
		RWLockWRGuard _(m_memory_region_lock);

		auto region = TRY(SharedMemoryObjectManager::get().shmat(shmid, shmaddr, shmflg));

		const vaddr_t region_vaddr = region->vaddr();

		TRY(add_mapped_region(BAN::move(region)));

		return region_vaddr;
	}

	BAN::ErrorOr<long> Process::sys_shmdt(const void* shmaddr)
	{
		const vaddr_t vaddr = reinterpret_cast<vaddr_t>(shmaddr);

		RWLockWRGuard _(m_memory_region_lock);

		const size_t idx = find_mapped_region(vaddr);
		if (idx >= m_mapped_regions.size() || m_mapped_regions[idx]->vaddr() != vaddr)
			return BAN::Error::from_errno(ENOENT);

		m_mapped_regions[idx]->wait_not_pinned();
		m_mapped_regions.remove(idx);

		return 0;
	}

	BAN::ErrorOr<long> Process::sys_ttyname(int fildes, char* user_buffer, size_t buffer_size)
	{
		auto inode = TRY(m_open_file_descriptors.inode_of(fildes));
		if (!inode->is_tty())
			return BAN::Error::from_errno(ENOTTY);

		auto path = TRY(m_open_file_descriptors.path_of(fildes));
		if (buffer_size < path.size() + 1)
			return BAN::Error::from_errno(ERANGE);

		TRY(write_to_user(user_buffer, path.data(), path.size() + 1));

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

	BAN::ErrorOr<long> Process::sys_ptsname(int fildes, char* user_buffer, size_t buffer_len)
	{
		auto file = TRY(m_open_file_descriptors.file_of(fildes));
		if (file.canonical_path != "<ptmx>"_sv)
			return BAN::Error::from_errno(ENOTTY);

		auto ptsname = TRY(static_cast<PseudoTerminalMaster*>(file.inode.ptr())->ptsname());
		if (buffer_len < ptsname.size() + 1)
			return BAN::Error::from_errno(ERANGE);

		TRY(write_to_user(user_buffer, ptsname.data(), ptsname.size() + 1));

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

	BAN::ErrorOr<long> Process::sys_clock_gettime(clockid_t clock_id, timespec* user_tp)
	{
		timespec tp;

		switch (clock_id)
		{
			case CLOCK_MONOTONIC:
				tp = SystemTimer::get().time_since_boot();
				break;
			case CLOCK_REALTIME:
				tp = SystemTimer::get().real_time();
				break;
			case CLOCK_PROCESS_CPUTIME_ID:
			{
				LockGuard _(m_process_lock);
				uint64_t cpu_time_ns { 0 };
				for (auto* thread : m_threads)
					cpu_time_ns += thread->cpu_time_total_ns();
				tp = {
					.tv_sec = static_cast<time_t>(cpu_time_ns / 1'000'000'000),
					.tv_nsec = static_cast<long>(cpu_time_ns % 1'000'000'000),
				};
				break;
			}
			case CLOCK_THREAD_CPUTIME_ID:
			{
				const auto cpu_time_ns = Thread::current().cpu_time_total_ns();
				tp = {
					.tv_sec = static_cast<time_t>(cpu_time_ns / 1'000'000'000),
					.tv_nsec = static_cast<long>(cpu_time_ns % 1'000'000'000),
				};
				break;
			}
			default:
				dwarnln("TODO: clock_gettime({})", clock_id);
				return BAN::Error::from_errno(ENOTSUP);
		}

		TRY(write_to_user(user_tp, &tp, sizeof(timespec)));

		return 0;
	}


	BAN::ErrorOr<long> Process::sys_load_keymap(const char* user_path)
	{
		char path[PATH_MAX];
		TRY(read_string_from_user(user_path, path, PATH_MAX));

		LockGuard _(m_process_lock);

		if (!m_credentials.is_superuser())
			return BAN::Error::from_errno(EPERM);

		auto absolute_path = TRY(absolute_path_of(path));
		TRY(LibInput::KeyboardLayout::get().load_from_file(absolute_path));

		return 0;
	}

	void Process::set_stopped(bool stopped, int signal)
	{
		SpinLockGuard _(m_signal_lock);

		Process* parent = nullptr;

		for_each_process(
			[&parent, this](Process& process) -> BAN::Iteration
			{
				if (process.pid() != m_parent)
					return BAN::Iteration::Continue;
				parent = &process;
				return BAN::Iteration::Break;
			}
		);

		if (parent != nullptr)
		{
			{
				SpinLockGuard _(parent->m_child_wait_lock);

				for (auto& child : parent->m_child_wait_statuses)
				{
					if (child.pid != pid())
						continue;
					if (!child.status.has_value() || WIFCONTINUED(*child.status) || WIFSTOPPED(*child.status))
						child.status = stopped
							? __WGENSTOPCODE(signal)
							: __WGENCONTCODE();
					break;
				}

				parent->m_child_wait_blocker.unblock();
			}

			if (!(m_signal_handlers[SIGCHLD].sa_flags & SA_NOCLDSTOP))
			{
				parent->add_pending_signal(SIGCHLD, {
					.si_signo = SIGCHLD,
					.si_code = stopped ? CLD_STOPPED : CLD_CONTINUED,
					.si_errno = 0,
					.si_pid = pid(),
					.si_uid = m_credentials.ruid(),
					.si_addr = nullptr,
					.si_status = __WGENEXITCODE(0, signal),
					.si_band = 0,
					.si_value = { .sival_int = 0 },
				});

				if (!parent->m_threads.empty())
					Processor::scheduler().unblock_thread(parent->m_threads.front());
			}
		}

		m_stopped = stopped;
		m_stop_blocker.unblock();
	}

	void Process::wait_while_stopped()
	{
		if (!m_stopped) [[likely]]
			return;

		for (;;)
		{
			while (Thread::current().will_exit_because_of_signal())
				Thread::current().handle_signal_if_interrupted();

			SpinLockGuard guard(m_signal_lock);
			if (!m_stopped)
				break;

			BlockableSpinLock block(m_signal_lock);
			m_stop_blocker.block_indefinite(&block);
		}
	}

	BAN::ErrorOr<void> Process::kill(pid_t pid, int signal, const siginfo_t& signal_info)
	{
		if (pid == 0 || pid == -1)
			return BAN::Error::from_errno(ENOTSUP);
		if (signal != 0 && (signal < _SIGMIN || signal > _SIGMAX))
			return BAN::Error::from_errno(EINVAL);

		bool found = false;
		for_each_process(
			[&](Process& process)
			{
				if (pid != process.pid() && -pid != process.pgrp())
					return BAN::Iteration::Continue;

				found = true;

				if (signal == 0)
					;
				// NOTE: stopped signals go through thread's signal handling code
				//       because for example SIGTSTP can be ignored
				else if (Thread::is_continuing_signal(signal))
					process.set_stopped(false, signal);
				else
				{
					process.add_pending_signal(signal, signal_info);
					if (!process.m_threads.empty())
						Processor::scheduler().unblock_thread(process.m_threads.front());
					process.m_stop_blocker.unblock();
				}

				return (pid > 0) ? BAN::Iteration::Break : BAN::Iteration::Continue;
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

		const siginfo_t signal_info {
			.si_signo = signal,
			.si_code = SI_USER,
			.si_errno = 0,
			.si_pid = this->pid(),
			.si_uid = m_credentials.ruid(),
			.si_addr = nullptr,
			.si_status = 0,
			.si_band = 0,
			.si_value = {},
		};

		if (pid == m_pid)
		{
			if (signal == 0)
				;
			// NOTE: stopped signals go through thread's signal handling code
			//       because for example SIGTSTP can be ignored
			else if (Thread::is_continuing_signal(signal))
				set_stopped(false, signal);
			else
			{
				add_pending_signal(signal, signal_info);
				m_stop_blocker.unblock();
			}
			return 0;
		}

		TRY(kill(pid, signal, signal_info));
		return 0;
	}

	BAN::ErrorOr<long> Process::sys_sigaction(int signal, const struct sigaction* user_act, struct sigaction* user_oact)
	{
		if (signal < _SIGMIN || signal > _SIGMAX)
			return BAN::Error::from_errno(EINVAL);

		struct sigaction new_act, old_act;
		if (user_act != nullptr)
			TRY(read_from_user(user_act, &new_act, sizeof(struct sigaction)));

		{
			SpinLockGuard _(m_signal_lock);
			old_act = m_signal_handlers[signal];
			if (user_act != nullptr)
				m_signal_handlers[signal] = new_act;
		}

		if (user_oact != nullptr)
			TRY(write_to_user(user_oact, &old_act, sizeof(struct sigaction)));

		return 0;
	}

	BAN::ErrorOr<long> Process::sys_sigpending(sigset_t* user_sigset)
	{
		sigset_t sigset;

		{
			auto& thread = Thread::current();
			SpinLockGuard _(thread.m_signal_lock);
			sigset = (signal_pending_mask() | thread.m_signal_pending_mask) & thread.m_signal_block_mask;
		}

		TRY(write_to_user(user_sigset, &sigset, sizeof(sigset_t)));
		return 0;
	}

	BAN::ErrorOr<long> Process::sys_sigprocmask(int how, const sigset_t* user_set, sigset_t* user_oset)
	{
		LockGuard _(m_process_lock);

		auto& thread = Thread::current();

		const sigset_t old_sigset = thread.m_signal_block_mask;

		if (user_set != nullptr)
		{
			sigset_t mask;
			TRY(read_from_user(user_set, &mask, sizeof(sigset_t)));
			mask &= ~((1ull << SIGKILL) | (1ull << SIGSTOP));

			SpinLockGuard _(thread.m_signal_lock);
			switch (how)
			{
				case SIG_BLOCK:
					thread.m_signal_block_mask |= mask;
					break;
				case SIG_UNBLOCK:
					thread.m_signal_block_mask &= ~mask;
					break;
				case SIG_SETMASK:
					thread.m_signal_block_mask = mask;
					break;
				default:
					return BAN::Error::from_errno(EINVAL);
			}
		}

		if (user_oset != nullptr)
			TRY(write_to_user(user_oset, &old_sigset, sizeof(sigset_t)));

		return 0;
	}

	BAN::ErrorOr<long> Process::sys_sigsuspend(const sigset_t* user_set)
	{
		sigset_t set;
		TRY(read_from_user(user_set, &set, sizeof(sigset_t)));

		LockGuard _(m_process_lock);

		auto& thread = Thread::current();
		thread.set_suspend_signal_mask(set & ~((1ull << SIGKILL) | (1ull << SIGSTOP)));

		// FIXME: i *think* here is a race condition as kill doesnt hold process lock
		while (!thread.is_interrupted_by_signal())
			Processor::scheduler().block_current_thread(nullptr, -1, &m_process_lock);

		return BAN::Error::from_errno(EINTR);
	}

	BAN::ErrorOr<long> Process::sys_sigwait(const sigset_t* user_set, int* user_signal)
	{
		sigset_t set;
		TRY(read_from_user(user_set, &set, sizeof(sigset_t)));

		LockGuard _(m_process_lock);

		auto& thread = Thread::current();
		for (;;)
		{
			BAN::Optional<int> signal;

			{
				SpinLockGuard _1(thread.m_signal_lock);
				SpinLockGuard _2(m_signal_lock);

				const uint64_t pending = thread.m_signal_pending_mask | this->m_signal_pending_mask;
				if (const auto wait_mask = pending & set)
				{
					for (size_t i = _SIGMIN; i <= _SIGMAX; i++)
					{
						const auto mask = 1ull << i;
						if (!(wait_mask & mask))
							continue;
						thread.m_signal_pending_mask &= ~mask;
						this->m_signal_pending_mask &= ~mask;
						signal = i;
						break;
					}

					ASSERT(signal.has_value());
				}
			}

			if (signal.has_value())
			{
				TRY(write_to_user(user_signal, &signal.value(), sizeof(int)));
				return 0;
			}

			// FIXME: i *think* here is a race condition as kill doesnt hold process lock
			Processor::scheduler().block_current_thread(nullptr, -1, &m_process_lock);
		}
	}

	BAN::ErrorOr<long> Process::sys_sigaltstack(const stack_t* user_ss, stack_t* user_oss)
	{
		stack_t ss, oss;
		if (user_ss != nullptr)
			TRY(read_from_user(user_ss, &ss, sizeof(stack_t)));

		TRY(Thread::current().sigaltstack(user_ss ? &ss : nullptr, user_oss ? &oss : nullptr));

		if (user_oss != nullptr)
			TRY(write_to_user(user_oss, &oss, sizeof(stack_t)));

		return 0;
	}

	BAN::ErrorOr<long> Process::sys_futex(int op, const uint32_t* addr, uint32_t val, const timespec* user_abstime)
	{
		const vaddr_t vaddr = reinterpret_cast<vaddr_t>(addr);
		if (vaddr % 4)
			return BAN::Error::from_errno(EINVAL);

		const bool is_realtime = (op & FUTEX_REALTIME);
		const bool is_private = (op & FUTEX_PRIVATE);
		op &= ~(FUTEX_PRIVATE | FUTEX_REALTIME);

		auto* buffer_region = TRY(validate_and_pin_pointer_access(addr, sizeof(uint32_t), false));
		BAN::ScopeGuard pin_guard([buffer_region] { buffer_region->unpin(); });

		const paddr_t paddr = m_page_table->physical_address_of(vaddr & PAGE_ADDR_MASK) | (vaddr & ~PAGE_ADDR_MASK);
		ASSERT(paddr != 0);

		switch (op)
		{
			case FUTEX_WAIT:
				break;
			case FUTEX_WAKE:
				user_abstime = nullptr;
				break;
			default:
				return BAN::Error::from_errno(ENOSYS);
		}

		if (op == FUTEX_WAIT && BAN::atomic_load(*addr) != val)
			return BAN::Error::from_errno(EAGAIN);

		uint64_t wake_time_ns = BAN::numeric_limits<uint64_t>::max();

		if (user_abstime != nullptr)
		{
			timespec abstime;
			TRY(read_from_user(user_abstime, &abstime, sizeof(timespec)));

			const uint64_t abstime_ns = abstime.tv_sec * 1'000'000'000 + abstime.tv_nsec;

			if (!is_realtime)
				wake_time_ns = abstime_ns;
			else
			{
				const auto realtime = SystemTimer::get().real_time();
				const uint64_t realtime_ns = realtime.tv_sec * 1'000'000'000 + realtime.tv_nsec;
				if (abstime_ns <= realtime_ns)
					return BAN::Error::from_errno(ETIMEDOUT);
				wake_time_ns = SystemTimer::get().ns_since_boot() + (abstime_ns - realtime_ns);
			}
		}

		futex_t* futex;

		{
			auto& futex_lock = is_private ? m_futex_lock : s_futex_lock;
			auto& futexes = is_private ? m_futexes : s_futexes;

			LockGuard _(futex_lock);

			auto it = futexes.find(paddr);
			if (it != futexes.end())
				futex = it->value.ptr();
			else switch (op)
			{
				case FUTEX_WAIT:
					futex = TRY(futexes.emplace(paddr, TRY(BAN::UniqPtr<futex_t>::create())))->value.ptr();
					break;
				case FUTEX_WAKE:
					return 0;
			}
		}

		LockGuard _(futex->mutex);

		switch (op)
		{
			case FUTEX_WAIT:
			{
				if (BAN::atomic_load(*addr) != val)
					return BAN::Error::from_errno(EAGAIN);

				futex->waiters++;

				// TODO: Deallocate unused futex objects at some point (?)
				//       We don't want to do it on every operation as allocation
				//       and deletion slows this down a lot and the same addresses
				//       will be probably used again
				BAN::ScopeGuard cleanup([futex] { futex->waiters--; });

				for (;;)
				{
					TRY(Thread::current().block_or_eintr_or_waketime_ns(futex->blocker, wake_time_ns, true, &futex->mutex));
					if (BAN::atomic_load(*addr) == val || futex->to_wakeup == 0)
						continue;
					futex->to_wakeup--;
					return 0;
				}
			}
			case FUTEX_WAKE:
			{
				if (BAN::Math::will_addition_overflow(futex->to_wakeup, val))
					futex->to_wakeup = BAN::numeric_limits<uint32_t>::max();
				else
					futex->to_wakeup += val;

				futex->to_wakeup = BAN::Math::min(futex->to_wakeup, futex->waiters);

				if (futex->to_wakeup > 0)
					futex->blocker.unblock();

				return 0;
			}
		}

		ASSERT_NOT_REACHED();
	}

	BAN::ErrorOr<long> Process::sys_yield()
	{
		Processor::yield();
		return 0;
	}

	BAN::ErrorOr<long> Process::sys_set_fsbase(void* addr)
	{
		auto& thread = Thread::current();
		thread.set_fsbase(reinterpret_cast<vaddr_t>(addr));
		Processor::load_fsbase();
		return 0;
	}

	BAN::ErrorOr<long> Process::sys_get_fsbase()
	{
		return Thread::current().get_fsbase();
	}

	BAN::ErrorOr<long> Process::sys_set_gsbase(void* addr)
	{
		auto& thread = Thread::current();
		thread.set_gsbase(reinterpret_cast<vaddr_t>(addr));
		Processor::load_gsbase();
		return 0;
	}

	BAN::ErrorOr<long> Process::sys_get_gsbase()
	{
		return Thread::current().get_gsbase();
	}

	BAN::ErrorOr<long> Process::sys_thread_create(void (*entry)(void*), void* arg, void* stack_base, size_t stack_size)
	{
		const vaddr_t stack_vaddr = reinterpret_cast<vaddr_t>(stack_base);
		if (stack_vaddr % PAGE_SIZE || stack_size % PAGE_SIZE)
			return BAN::Error::from_errno(EINVAL);

		auto* memory_region = TRY(validate_and_pin_pointer_access(stack_base, stack_size, true));
		BAN::ScopeGuard _0([memory_region] { if (memory_region) memory_region->unpin(); });

		const vaddr_t initial_stack_pointer = stack_vaddr + stack_size - sizeof(void*);
		*reinterpret_cast<void**>(initial_stack_pointer) = arg;

		auto* thread = TRY(Thread::create_userspace(
			this,
			page_table(),
			stack_vaddr,
			stack_size,
			reinterpret_cast<vaddr_t>(entry),
			initial_stack_pointer
		));
		thread->m_signal_block_mask = Thread::current().m_signal_block_mask;

		LockGuard _1(m_process_lock);

		TRY(m_threads.push_back(thread));
		if (auto ret = Processor::scheduler().add_thread(thread); ret.is_error())
		{
			m_threads.pop_back();
			delete thread;
			return ret.release_error();
		}

		return thread->tid();
	}

	BAN::ErrorOr<long> Process::sys_thread_exit(void* value)
	{
		{
			LockGuard _(m_process_lock);

			if (m_threads.size() == 1)
				return sys_exit(0);

			auto& thread = Thread::current();
			if (!thread.is_detached())
			{
				TRY(m_exited_threads.emplace_back(thread.tid(), value));
				m_thread_exit_blocker.unblock();
			}
		}

		Thread::current().on_exit();

		ASSERT_NOT_REACHED();
	}

	BAN::ErrorOr<long> Process::sys_thread_join(pid_t tid, void** user_value)
	{
		LockGuard _(m_process_lock);

		if (tid == Thread::current().tid())
			return BAN::Error::from_errno(EINVAL);

		const auto check_thread =
			[&]() -> BAN::Optional<void*>
			{
				for (size_t i = 0; i < m_exited_threads.size(); i++)
				{
					if (m_exited_threads[i].tid != tid)
						continue;
					void* ret = m_exited_threads[i].value;
					m_exited_threads.remove(i);
					return ret;
				}

				return {};
			};

		for (;;)
		{
			if (auto ret = check_thread(); ret.has_value())
			{
				if (user_value != nullptr)
					TRY(write_to_user(user_value, &ret.value(), sizeof(void*)));
				return 0;
			}

			{
				bool found = false;
				bool joinable = false;
				for (auto* thread : m_threads)
				{
					if (thread->tid() != tid)
						continue;
					found = true;
					joinable = !thread->is_detached();
					break;
				}
				if (!found)
					return BAN::Error::from_errno(ESRCH);
				if (!joinable)
					return BAN::Error::from_errno(EINVAL);
			}

			TRY(Thread::current().block_or_eintr_indefinite(m_thread_exit_blocker, &m_process_lock));
		}
	}

	BAN::ErrorOr<long> Process::sys_thread_getid()
	{
		return Thread::current().tid();
	}

	BAN::ErrorOr<long> Process::sys_thread_kill(pid_t tid, int signal)
	{
		if (signal != 0 && (signal < _SIGMIN || signal > _SIGMAX))
			return BAN::Error::from_errno(EINVAL);

		LockGuard _(m_process_lock);

		for (auto* thread : m_threads)
		{
			if (thread->tid() != tid)
				continue;
			if (signal == 0)
				return 0;
			thread->add_signal(signal, {
				.si_signo = signal,
				.si_code = SI_USER,
				.si_errno = 0,
				.si_pid = pid(),
				.si_uid = m_credentials.ruid(),
				.si_addr = nullptr,
				.si_status = 0,
				.si_band = 0,
				.si_value = {},
			});
			return 0;
		}

		return BAN::Error::from_errno(ESRCH);
	}

	BAN::ErrorOr<long> Process::sys_thread_detach(pid_t tid)
	{
		LockGuard _(m_process_lock);

		for (auto* thread : m_threads)
		{
			if (thread->tid() != tid)
				continue;
			if (thread->is_detached())
				return BAN::Error::from_errno(EINVAL);
			thread->detach();
			m_thread_exit_blocker.unblock();
			return 0;
		}

		return BAN::Error::from_errno(ESRCH);
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

	BAN::ErrorOr<long> Process::sys_getgroups(gid_t user_groups[], size_t count)
	{
		if (BAN::Math::will_multiplication_overflow(count, sizeof(gid_t)))
			return BAN::Error::from_errno(EOVERFLOW);

		LockGuard _(m_process_lock);

		const auto current = m_credentials.groups();
		if (count == 0)
			return current.size();

		if (current.size() > count)
			return BAN::Error::from_errno(EINVAL);

		TRY(write_to_user(user_groups, current.data(), current.size() * sizeof(gid_t)));

		return current.size();
	}

	BAN::ErrorOr<long> Process::sys_setgroups(const gid_t groups[], size_t count)
	{
		if (BAN::Math::will_multiplication_overflow(count, sizeof(gid_t)))
			return BAN::Error::from_errno(EOVERFLOW);

		LockGuard _(m_process_lock);

		if (!m_credentials.is_superuser())
			return BAN::Error::from_errno(EPERM);

		auto* region = TRY(validate_and_pin_pointer_access(groups, count * sizeof(gid_t), false));
		BAN::ScopeGuard pin_guard([region] { region->unpin(); });

		TRY(m_credentials.set_groups({ groups, count }));

		return 0;
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

	BAN::ErrorOr<bool> Process::allocate_page_for_demand_paging(vaddr_t address, bool wants_write, bool wants_exec)
	{
		ASSERT(&Process::current() == this);

		const auto is_allocated =
			[&]() -> bool
			{
				auto wanted_flags = PageTable::Flags::UserSupervisor | PageTable::Flags::Present;
				if (wants_write)
					wanted_flags |= PageTable::Flags::ReadWrite;
				if (wants_exec)
					wanted_flags |= PageTable::Flags::Execute;
				if ((m_page_table->get_page_flags(address & PAGE_ADDR_MASK) & wanted_flags) == wanted_flags)
					return true;
				return false;
			};

		{
			RWLockRDGuard _(m_memory_region_lock);
			if (is_allocated())
				return true;
		}

		RWLockWRGuard _(m_memory_region_lock);

		if (is_allocated())
			return true;

		const size_t index = find_mapped_region(address);
		if (index >= m_mapped_regions.size())
			return false;

		auto& region = m_mapped_regions[index];
		if (!region->contains(address))
			return false;

		return region->allocate_page_containing(address, wants_write);
	}

	BAN::ErrorOr<MemoryRegion*> Process::validate_and_pin_pointer_access(const void* ptr, size_t size, bool needs_write)
	{
		// TODO: allow pinning multiple regions?

		const vaddr_t user_vaddr = reinterpret_cast<vaddr_t>(ptr);

		{
			RWLockRDGuard _(m_memory_region_lock);

			const size_t first_index = find_mapped_region(user_vaddr);
			for (size_t i = first_index; i < m_mapped_regions.size(); i++)
			{
				auto& region = m_mapped_regions[i];
				if (user_vaddr >= region->vaddr() + region->size())
					break;
				if (!region->contains_fully(user_vaddr, size))
					continue;

				const size_t page_count = range_page_count(user_vaddr, size);
				const vaddr_t page_base = user_vaddr & PAGE_ADDR_MASK;
				for (size_t p = 0; p < page_count; p++)
				{
					const auto flags = PageTable::UserSupervisor | (needs_write ? PageTable::ReadWrite : 0) | PageTable::Present;
					if ((m_page_table->get_page_flags(page_base + p * PAGE_SIZE) & flags) != flags)
						goto validate_and_pin_pointer_access_with_allocation;
				}

				region->pin();
				return region.ptr();
			}

			return BAN::Error::from_errno(EFAULT);
		}

	validate_and_pin_pointer_access_with_allocation:
		RWLockWRGuard _(m_memory_region_lock);

		const size_t first_index = find_mapped_region(user_vaddr);
		for (size_t i = first_index; i < m_mapped_regions.size(); i++)
		{
			auto& region = m_mapped_regions[i];
			if (user_vaddr >= region->vaddr() + region->size())
				break;
			if (!region->contains_fully(user_vaddr, size))
				continue;

			const size_t page_count = range_page_count(user_vaddr, size);
			const vaddr_t page_base = user_vaddr & PAGE_ADDR_MASK;
			for (size_t p = 0; p < page_count; p++)
			{
				const auto flags = PageTable::UserSupervisor | (needs_write ? PageTable::ReadWrite : 0) | PageTable::Present;
				if ((m_page_table->get_page_flags(page_base + p * PAGE_SIZE) & flags) == flags)
					continue;
				if (!TRY(region->allocate_page_containing(page_base + p * PAGE_SIZE, needs_write)))
					return BAN::Error::from_errno(EFAULT);
			}

			region->pin();
			return region.ptr();
		}

		return BAN::Error::from_errno(EFAULT);
	}

	BAN::ErrorOr<long> Process::sys_banos_install(const char* u_image) {
		return TRY(Banos::load_driver_from_image(u_image));
	}
}
