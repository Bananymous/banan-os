#include <BAN/Errors.h>
#include <BAN/ScopeGuard.h>
#include <kernel/GDT.h>
#include <kernel/InterruptController.h>
#include <kernel/InterruptStack.h>
#include <kernel/Memory/kmalloc.h>
#include <kernel/Process.h>
#include <kernel/Scheduler.h>
#include <kernel/Thread.h>
#include <kernel/Timer/Timer.h>

namespace Kernel
{

	extern "C" [[noreturn]] void start_kernel_thread();
	extern "C" [[noreturn]] void start_userspace_thread();

	extern "C" void signal_trampoline();

	template<typename T>
	static void write_to_stack(uintptr_t& rsp, const T& value) requires(sizeof(T) <= sizeof(uintptr_t))
	{
		rsp -= sizeof(uintptr_t);
		*(uintptr_t*)rsp = (uintptr_t)value;
	}

	extern "C" uintptr_t get_thread_start_sp()
	{
		return Thread::current().interrupt_stack().sp;
	}

	extern "C" void load_thread_sse()
	{
		Thread::current().load_sse();
	}

	static pid_t s_next_tid = 1;

	alignas(16) static uint8_t s_default_sse_storage[512];
	static BAN::Atomic<bool> s_default_sse_storage_initialized = false;

	static void initialize_default_sse_storage()
	{
		static BAN::Atomic<bool> is_initializing { false };
		bool expected { false };
		if (!is_initializing.compare_exchange(expected, true))
		{
			while (!s_default_sse_storage_initialized)
				__builtin_ia32_pause();
			asm volatile("" ::: "memory");
			return;
		}

		const uint32_t mxcsr = 0x1F80;
		asm volatile(
			"finit;"
			"ldmxcsr %[mxcsr];"
#if ARCH(x86_64)
			"fxsave64 %[storage];"
#elif ARCH(i686)
			"fxsave %[storage];"
#else
#error
#endif
			: [storage]"=m"(s_default_sse_storage)
			: [mxcsr]"m"(mxcsr)
		);

		s_default_sse_storage_initialized = true;
	}

	bool Thread::is_stopping_signal(int signal)
	{
		switch(signal)
		{
			case SIGSTOP:
			case SIGTSTP:
			case SIGTTIN:
			case SIGTTOU:
				return true;
			default:
				return false;
		}
	}

	bool Thread::is_continuing_signal(int signal)
	{
		switch(signal)
		{
			case SIGCONT:
				return true;
			default:
				return false;
		}
	}

	bool Thread::is_terminating_signal(int signal)
	{
		switch (signal)
		{
			case SIGALRM:
			case SIGHUP:
			case SIGINT:
			case SIGKILL:
			case SIGPIPE:
			case SIGTERM:
			case SIGUSR1:
			case SIGUSR2:
			case SIGPOLL:
			case SIGPROF:
			case SIGVTALRM:
				return true;
			default:
				return false;
		}
	}

	bool Thread::is_abnormal_terminating_signal(int signal)
	{
		switch (signal)
		{
			case SIGABRT:
			case SIGBUS:
			case SIGFPE:
			case SIGILL:
			case SIGQUIT:
			case SIGSEGV:
			case SIGSYS:
			case SIGTRAP:
			case SIGXCPU:
			case SIGXFSZ:
				return true;
			default:
				return false;
		}
	}

	bool Thread::is_realtime_signal(int signal)
	{
		return SIGRTMIN <= signal && signal <= SIGRTMAX;
	}

	static bool is_default_ignored_signal(int signal)
	{
		switch (signal)
		{
			case SIGCHLD:
			case SIGURG:
			case SIGWINCH:
			case SIGCANCEL:
				return true;
			default:
				return Thread::is_realtime_signal(signal);
		}
	}

	BAN::ErrorOr<Thread*> Thread::create_kernel(entry_t entry, void* data)
	{
		// Create the thread object
		Thread* thread = new Thread(s_next_tid++, nullptr);
		if (thread == nullptr)
			return BAN::Error::from_errno(ENOMEM);
		BAN::ScopeGuard thread_deleter([thread] { delete thread; });

		// Initialize stack and registers
		thread->m_kernel_stack = TRY(VirtualRange::create_to_vaddr_range(
			PageTable::kernel(),
			KERNEL_OFFSET,
			~(uintptr_t)0,
			kernel_stack_size,
			PageTable::Flags::ReadWrite | PageTable::Flags::Present,
			true, true
		));

		// Initialize stack for returning
		uintptr_t sp = thread->kernel_stack_top();
		write_to_stack(sp, thread);
		write_to_stack(sp, &Thread::on_exit_trampoline);
		write_to_stack(sp, data);
		write_to_stack(sp, entry);

		thread->m_interrupt_stack.ip = reinterpret_cast<vaddr_t>(start_kernel_thread);
		thread->m_interrupt_stack.cs = 0x08;
		thread->m_interrupt_stack.flags = 0x002;
		thread->m_interrupt_stack.sp = sp;
		thread->m_interrupt_stack.ss = 0x10;

		memset(&thread->m_interrupt_registers, 0, sizeof(InterruptRegisters));

		thread_deleter.disable();

		return thread;
	}

	BAN::ErrorOr<Thread*> Thread::create_userspace(Process* process, PageTable& page_table)
	{
		ASSERT(process);

		// Create the thread object
		Thread* thread = new Thread(s_next_tid++, process);
		if (thread == nullptr)
			return BAN::Error::from_errno(ENOMEM);
		BAN::ScopeGuard thread_deleter([thread] { delete thread; });

		thread->m_is_userspace = true;

#if ARCH(x86_64)
		static constexpr vaddr_t stack_addr_start = 0x0000700000000000;
#elif ARCH(i686)
		static constexpr vaddr_t stack_addr_start = 0xB0000000;
#endif

		thread->m_kernel_stack = TRY(VirtualRange::create_to_vaddr_range(
			page_table,
			stack_addr_start, USERSPACE_END,
			kernel_stack_size,
			PageTable::Flags::ReadWrite | PageTable::Flags::Present,
			true, true
		));

		thread->m_userspace_stack = TRY(VirtualRange::create_to_vaddr_range(
			page_table,
			stack_addr_start, USERSPACE_END,
			userspace_stack_size,
			PageTable::Flags::UserSupervisor | PageTable::Flags::ReadWrite | PageTable::Flags::Present,
			false, true
		));

		thread_deleter.disable();

		return thread;
	}

	Thread::Thread(pid_t tid, Process* process)
		: m_tid(tid), m_process(process)
	{
		if (!s_default_sse_storage_initialized)
			initialize_default_sse_storage();
		memcpy(m_sse_storage, s_default_sse_storage, sizeof(m_sse_storage));
	}

	Thread& Thread::current()
	{
		return Processor::scheduler().current_thread();
	}

	pid_t Thread::current_tid()
	{
		if (Processor::count() == 0)
			return 0;
		return Processor::scheduler().current_tid();
	}

	Process& Thread::process()
	{
		ASSERT(m_process);
		return *m_process;
	}

	const Process& Thread::process() const
	{
		ASSERT(m_process);
		return *m_process;
	}

	Thread::~Thread()
	{
		if (m_delete_process)
		{
			ASSERT(m_process);
			delete m_process;
		}
	}

	uint64_t Thread::cpu_time_ns() const
	{
		SpinLockGuard _(m_cpu_time_lock);
		if (m_cpu_time_start_ns == UINT64_MAX)
			return m_cpu_time_ns;
		return m_cpu_time_ns + (SystemTimer::get().ns_since_boot() - m_cpu_time_start_ns);
	}

	void Thread::set_cpu_time_start()
	{
		SpinLockGuard _(m_cpu_time_lock);
		ASSERT(m_cpu_time_start_ns == UINT64_MAX);
		m_cpu_time_start_ns = SystemTimer::get().ns_since_boot();
	}

	void Thread::set_cpu_time_stop()
	{
		SpinLockGuard _(m_cpu_time_lock);
		ASSERT(m_cpu_time_start_ns != UINT64_MAX);
		m_cpu_time_ns += SystemTimer::get().ns_since_boot() - m_cpu_time_start_ns;
		m_cpu_time_start_ns = UINT64_MAX;
	}

	BAN::ErrorOr<Thread*> Thread::pthread_create(entry_t entry, void* arg)
	{
		auto* thread = TRY(create_userspace(m_process, m_process->page_table()));

		save_sse();
		memcpy(thread->m_sse_storage, m_sse_storage, sizeof(m_sse_storage));

		TRY(thread->userspace_stack().allocate_page_for_demand_paging(thread->userspace_stack_top() - PAGE_SIZE));
		PageTable::with_fast_page(thread->userspace_stack().paddr_of(thread->userspace_stack_top() - PAGE_SIZE), [=] {
			PageTable::fast_page_as<void*>(PAGE_SIZE - sizeof(uintptr_t)) = arg;
		});

		const vaddr_t entry_addr = reinterpret_cast<vaddr_t>(entry);
		thread->setup_exec(entry_addr, thread->userspace_stack_top() - sizeof(uintptr_t));

		return thread;
	}

	BAN::ErrorOr<Thread*> Thread::clone(Process* new_process, uintptr_t sp, uintptr_t ip)
	{
		ASSERT(m_is_userspace);
		ASSERT(m_state == State::Executing);

		Thread* thread = new Thread(s_next_tid++, new_process);
		if (thread == nullptr)
			return BAN::Error::from_errno(ENOMEM);
		BAN::ScopeGuard thread_deleter([thread] { delete thread; });

		thread->m_is_userspace = true;

		thread->m_kernel_stack = TRY(m_kernel_stack->clone(new_process->page_table()));
		thread->m_userspace_stack = TRY(m_userspace_stack->clone(new_process->page_table()));

		thread->m_tls = m_tls;

		thread->m_state = State::NotStarted;

		thread->m_interrupt_stack.ip = ip;
		thread->m_interrupt_stack.cs = 0x08;
		thread->m_interrupt_stack.flags = 0x002;
		thread->m_interrupt_stack.sp = sp;
		thread->m_interrupt_stack.ss = 0x10;

		save_sse();
		memcpy(thread->m_sse_storage, m_sse_storage, sizeof(m_sse_storage));

#if ARCH(x86_64)
		thread->m_interrupt_registers.rax = 0;
#elif ARCH(i686)
		thread->m_interrupt_registers.eax = 0;
#endif

		thread_deleter.disable();

		return thread;
	}

	BAN::ErrorOr<void> Thread::initialize_userspace(vaddr_t entry, BAN::Span<BAN::String> argv, BAN::Span<BAN::String> envp, BAN::Span<LibELF::AuxiliaryVector> auxv)
	{
		// System V ABI: Initial process stack

		ASSERT(m_is_userspace);
		ASSERT(m_userspace_stack);

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

		if (needed_size > m_userspace_stack->size())
			return BAN::Error::from_errno(ENOBUFS);

		vaddr_t vaddr = userspace_stack_top() - needed_size;

		const size_t page_count = BAN::Math::div_round_up(needed_size, PAGE_SIZE);
		for (size_t i = 0; i < page_count; i++)
			TRY(m_userspace_stack->allocate_page_for_demand_paging(vaddr + i * PAGE_SIZE));

		const auto stack_copy_buf =
			[this](BAN::ConstByteSpan buffer, vaddr_t vaddr) -> void
			{
				ASSERT(vaddr + buffer.size() <= userspace_stack_top());

				size_t bytes_copied = 0;
				while (bytes_copied < buffer.size())
				{
					const size_t to_copy = BAN::Math::min<size_t>(buffer.size() - bytes_copied, PAGE_SIZE - (vaddr % PAGE_SIZE));

					PageTable::with_fast_page(userspace_stack().paddr_of(vaddr & PAGE_ADDR_MASK), [=]() {
						memcpy(PageTable::fast_page_as_ptr(vaddr % PAGE_SIZE), buffer.data() + bytes_copied, to_copy);
					});

					vaddr += to_copy;
					bytes_copied += to_copy;
				}
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

		setup_exec(entry, userspace_stack_top() - needed_size);

		return {};
	}

	void Thread::setup_exec(vaddr_t ip, vaddr_t sp)
	{
		ASSERT(is_userspace());
		m_state = State::NotStarted;

		// Signal mask is inherited

		// Initialize stack for returning
		PageTable::with_fast_page(kernel_stack().paddr_of(kernel_stack_top() - PAGE_SIZE), [=] {
			uintptr_t cur_sp = PageTable::fast_page() + PAGE_SIZE;
			write_to_stack(cur_sp, 0x20 | 3);
			write_to_stack(cur_sp, sp);
			write_to_stack(cur_sp, 0x202);
			write_to_stack(cur_sp, 0x18 | 3);
			write_to_stack(cur_sp, ip);
		});

		m_interrupt_stack.ip = reinterpret_cast<vaddr_t>(start_userspace_thread);
		m_interrupt_stack.cs = 0x08;
		m_interrupt_stack.flags = 0x002;
		m_interrupt_stack.sp = kernel_stack_top() - 5 * sizeof(uintptr_t);
		m_interrupt_stack.ss = 0x10;

		memset(&m_interrupt_registers, 0, sizeof(InterruptRegisters));
	}

	void Thread::setup_process_cleanup()
	{
		ASSERT(Processor::get_interrupt_state() == InterruptState::Disabled);

		m_state = State::NotStarted;
		static entry_t entry(
			[](void* process_ptr)
			{
				auto* thread = &Thread::current();
				auto* process = static_cast<Process*>(process_ptr);

				ASSERT(thread->m_process == process);

				process->cleanup_function(thread);

				thread->m_delete_process = true;

				// will call on thread exit after return
			}
		);

		m_signal_pending_mask = 0;
		m_signal_block_mask = ~0ull;

		PageTable::with_fast_page(kernel_stack().paddr_of(kernel_stack_top() - PAGE_SIZE), [&] {
			uintptr_t sp = PageTable::fast_page() + PAGE_SIZE;
			write_to_stack(sp, this);
			write_to_stack(sp, &Thread::on_exit_trampoline);
			write_to_stack(sp, m_process);
			write_to_stack(sp, entry);
		});

		m_interrupt_stack.ip = reinterpret_cast<vaddr_t>(start_kernel_thread);
		m_interrupt_stack.cs = 0x08;
		m_interrupt_stack.flags = 0x002;
		m_interrupt_stack.sp = kernel_stack_top() - 4 * sizeof(uintptr_t);
		m_interrupt_stack.ss = 0x10;

		memset(&m_interrupt_registers, 0, sizeof(InterruptRegisters));
	}

	bool Thread::is_interrupted_by_signal(bool skip_stop_and_cont) const
	{
		if (!is_userspace() || m_state != State::Executing)
			return false;
		auto& interrupt_stack = *reinterpret_cast<InterruptStack*>(kernel_stack_top() - sizeof(InterruptStack));
		if (!GDT::is_user_segment(interrupt_stack.cs))
			return false;

		uint64_t full_pending_mask = m_signal_pending_mask | process().signal_pending_mask();
		uint64_t signals = full_pending_mask & ~m_signal_block_mask;
		for (uint8_t i = 0; i < _SIGMAX; i++)
		{
			if (!(signals & ((uint64_t)1 << i)))
				continue;
			if (skip_stop_and_cont && (is_stopping_signal(i) || is_continuing_signal(i)))
				continue;

			vaddr_t signal_handler;
			{
				SpinLockGuard _(m_process->m_signal_lock);
				ASSERT(!(m_process->m_signal_handlers[i].sa_flags & SA_SIGINFO));
				signal_handler = (vaddr_t)m_process->m_signal_handlers[i].sa_handler;
			}
			if (signal_handler == (vaddr_t)SIG_IGN)
				continue;
			if (signal_handler == (vaddr_t)SIG_DFL && is_default_ignored_signal(i))
				continue;
			return true;
		}
		return false;
	}

	bool Thread::can_add_signal_to_execute() const
	{
		return is_interrupted_by_signal() && m_mutex_count == 0;
	}

	bool Thread::will_execute_signal() const
	{
		if (!is_userspace() || m_state != State::Executing)
			return false;
		auto& interrupt_stack = *reinterpret_cast<InterruptStack*>(kernel_stack_top() - sizeof(InterruptStack));
		return interrupt_stack.ip == (uintptr_t)signal_trampoline;
	}

	bool Thread::will_exit_because_of_signal() const
	{
		const uint64_t full_pending_mask = m_signal_pending_mask | process().signal_pending_mask();
		const uint64_t signals = full_pending_mask & ~m_signal_block_mask;
		for (size_t sig = _SIGMIN; sig <= _SIGMAX; sig++)
			if (signals & (static_cast<uint64_t>(1) << sig))
				if (is_terminating_signal(sig) || is_abnormal_terminating_signal(sig))
					return true;
		return false;
	}

	bool Thread::handle_signal(int signal)
	{
		ASSERT(&Thread::current() == this);
		ASSERT(is_userspace());

		auto state = m_signal_lock.lock();

		auto& interrupt_stack = *reinterpret_cast<InterruptStack*>(kernel_stack_top() - sizeof(InterruptStack));
		ASSERT(GDT::is_user_segment(interrupt_stack.cs));

		if (signal == 0)
		{
			uint64_t full_pending_mask = m_signal_pending_mask | process().signal_pending_mask();
			for (signal = _SIGMIN; signal <= _SIGMAX; signal++)
			{
				uint64_t mask = 1ull << signal;
				if ((full_pending_mask & mask) && !(m_signal_block_mask & mask))
					break;
			}
			ASSERT(signal <= _SIGMAX);
		}
		else
		{
			ASSERT(signal >= _SIGMIN);
			ASSERT(signal <= _SIGMAX);
		}

		vaddr_t signal_handler;
		bool has_sa_restart;
		vaddr_t signal_stack_top = 0;
		{
			SpinLockGuard _(m_process->m_signal_lock);

			auto& handler = m_process->m_signal_handlers[signal];

			ASSERT(!(handler.sa_flags & SA_SIGINFO));

			signal_handler = reinterpret_cast<vaddr_t>(handler.sa_handler);
			if (handler.sa_flags & SA_RESETHAND)
				handler.sa_handler = SIG_DFL;

			has_sa_restart = !!(handler.sa_flags & SA_RESTART);

			const auto& alt_stack = m_signal_alt_stack;
			if (alt_stack.ss_flags != SS_DISABLE && (handler.sa_flags & SA_ONSTACK) && !currently_on_alternate_stack())
				signal_stack_top = reinterpret_cast<vaddr_t>(alt_stack.ss_sp) + alt_stack.ss_size;
		}

		m_signal_pending_mask &= ~(1ull << signal);
		process().remove_pending_signal(signal);

		if (m_signal_suspend_mask.has_value())
		{
			m_signal_block_mask = m_signal_suspend_mask.value();
			m_signal_suspend_mask.clear();
		}

		m_signal_lock.unlock(state);

		if (signal_handler == (vaddr_t)SIG_IGN)
			;
		else if (signal_handler != (vaddr_t)SIG_DFL)
		{
			// call userspace signal handlers
#if ARCH(x86_64)
			interrupt_stack.sp -= 128; // skip possible red-zone
#endif

			{
				// Make sure stack is allocated

				vaddr_t pages[3] {};
				size_t page_count { 0 };

				if (signal_stack_top == 0)
				{
					pages[0] = (interrupt_stack.sp - 1 * sizeof(uintptr_t)) & PAGE_ADDR_MASK;
					pages[1] = (interrupt_stack.sp - 5 * sizeof(uintptr_t)) & PAGE_ADDR_MASK;
					page_count = 2;
				}
				else
				{
					pages[0] = (interrupt_stack.sp - sizeof(uintptr_t)) & PAGE_ADDR_MASK;
					pages[1] = (signal_stack_top - 4 * sizeof(uintptr_t)) & PAGE_ADDR_MASK;
					pages[2] = (signal_stack_top - 1 * sizeof(uintptr_t)) & PAGE_ADDR_MASK;
					page_count = 3;
				}

				for (size_t i = 0; i < page_count; i++)
				{
					if (m_process->page_table().get_page_flags(pages[i]) & PageTable::Flags::Present)
						continue;
					Processor::set_interrupt_state(InterruptState::Enabled);
					if (auto ret = m_process->allocate_page_for_demand_paging(pages[i], true, false); ret.is_error() || !ret.value())
						m_process->exit(128 + SIGSEGV, SIGSEGV);
					Processor::set_interrupt_state(InterruptState::Disabled);
				}
			}

			write_to_stack(interrupt_stack.sp, interrupt_stack.ip);
			const vaddr_t old_stack = interrupt_stack.sp;
			if (signal_stack_top)
				interrupt_stack.sp = signal_stack_top;
			write_to_stack(interrupt_stack.sp, old_stack);
			write_to_stack(interrupt_stack.sp, interrupt_stack.flags);
			write_to_stack(interrupt_stack.sp, signal);
			write_to_stack(interrupt_stack.sp, signal_handler);
			interrupt_stack.ip = (uintptr_t)signal_trampoline;
		}
		else
		{
			if (is_abnormal_terminating_signal(signal))
			{
				process().exit(128 + signal, signal | 0x80);
				ASSERT_NOT_REACHED();
			}
			else if (is_terminating_signal(signal))
			{
				process().exit(128 + signal, signal);
				ASSERT_NOT_REACHED();
			}
			else if (is_stopping_signal(signal))
			{
				process().set_stopped(true, signal);
			}
			else if (is_continuing_signal(signal))
			{
				process().set_stopped(false, signal);
			}
			else if (is_default_ignored_signal(signal))
			{

			}
			else
			{
				panic("Executing unhandled signal {}", signal);
			}
		}

		return has_sa_restart;
	}

	void Thread::add_signal(int signal)
	{
		SpinLockGuard _(m_signal_lock);
		if (m_process)
		{
			vaddr_t signal_handler;
			{
				SpinLockGuard _(m_process->m_signal_lock);
				ASSERT(!(m_process->m_signal_handlers[signal].sa_flags & SA_SIGINFO));
				signal_handler = (vaddr_t)m_process->m_signal_handlers[signal].sa_handler;
			}
			if (signal_handler == (vaddr_t)SIG_IGN)
				return;
			if (signal_handler == (vaddr_t)SIG_DFL && is_default_ignored_signal(signal))
				return;
		}

		const uint64_t mask = 1ull << signal;
		m_signal_pending_mask |= mask;

		if (this != &Thread::current())
			Processor::scheduler().unblock_thread(this);
	}

	void Thread::set_suspend_signal_mask(uint64_t sigmask)
	{
		SpinLockGuard _(m_signal_lock);
		ASSERT(!m_signal_suspend_mask.has_value());
		m_signal_suspend_mask = m_signal_block_mask;
		m_signal_block_mask = sigmask;
	}

	bool Thread::currently_on_alternate_stack() const
	{
		ASSERT(m_signal_lock.current_processor_has_lock());

		if (m_signal_alt_stack.ss_flags == SS_ONSTACK)
			return false;

		const vaddr_t stack_bottom = reinterpret_cast<vaddr_t>(m_signal_alt_stack.ss_sp);
		const vaddr_t stack_top = stack_bottom + m_signal_alt_stack.ss_size;
		const vaddr_t sp = m_interrupt_stack.sp;
		return stack_bottom <= sp && sp <= stack_top;
	}

	BAN::ErrorOr<void> Thread::sigaltstack(const stack_t* ss, stack_t* oss)
	{
		SpinLockGuard _(m_signal_lock);

		const bool on_alt_stack = currently_on_alternate_stack();

		if (oss)
		{
			*oss = m_signal_alt_stack;
			if (on_alt_stack)
				oss->ss_flags = SS_ONSTACK;
		}

		if (ss)
		{
			if (on_alt_stack)
				return BAN::Error::from_errno(EPERM);
			if (ss->ss_flags && ss->ss_flags != SS_DISABLE)
				return BAN::Error::from_errno(EINVAL);
			if (ss->ss_size < MINSIGSTKSZ)
				return BAN::Error::from_errno(ENOMEM);
			m_signal_alt_stack = *ss;
		}

		return {};
	}

	BAN::ErrorOr<void> Thread::sleep_or_eintr_ns(uint64_t ns)
	{
		if (is_interrupted_by_signal(true))
			return BAN::Error::from_errno(EINTR);
		SystemTimer::get().sleep_ns(ns);
		if (is_interrupted_by_signal(true))
			return BAN::Error::from_errno(EINTR);
		return {};
	}

	BAN::ErrorOr<void> Thread::block_or_eintr_indefinite(ThreadBlocker& thread_blocker, BaseMutex* mutex)
	{
		if (is_interrupted_by_signal(true))
			return BAN::Error::from_errno(EINTR);
		thread_blocker.block_indefinite(mutex);
		if (is_interrupted_by_signal(true))
			return BAN::Error::from_errno(EINTR);
		return {};
	}

	BAN::ErrorOr<void> Thread::block_or_eintr_or_timeout_ns(ThreadBlocker& thread_blocker, uint64_t timeout_ns, bool etimedout, BaseMutex* mutex)
	{
		const uint64_t wake_time_ns = SystemTimer::get().ns_since_boot() + timeout_ns;
		return block_or_eintr_or_waketime_ns(thread_blocker, wake_time_ns, etimedout, mutex);
	}

	BAN::ErrorOr<void> Thread::block_or_eintr_or_waketime_ns(ThreadBlocker& thread_blocker, uint64_t wake_time_ns, bool etimedout, BaseMutex* mutex)
	{
		if (is_interrupted_by_signal(true))
			return BAN::Error::from_errno(EINTR);
		thread_blocker.block_with_wake_time_ns(wake_time_ns, mutex);
		if (is_interrupted_by_signal(true))
			return BAN::Error::from_errno(EINTR);
		if (etimedout && SystemTimer::get().ms_since_boot() >= wake_time_ns)
			return BAN::Error::from_errno(ETIMEDOUT);
		return {};
	}

	void Thread::on_exit_trampoline(Thread* thread)
	{
		thread->on_exit();
	}

	void Thread::on_exit()
	{
		ASSERT(this == &Thread::current());
		if (!m_delete_process && has_process())
		{
			if (process().on_thread_exit(*this))
			{
				Processor::set_interrupt_state(InterruptState::Disabled);
				setup_process_cleanup();
				Processor::yield();
				ASSERT_NOT_REACHED();
			}
		}

		Processor::set_interrupt_state(InterruptState::Disabled);
		m_state = State::Terminated;
		Processor::yield();
		ASSERT_NOT_REACHED();
	}

	void Thread::save_sse()
	{
#if ARCH(x86_64)
		asm volatile("fxsave64 %0" :: "m"(m_sse_storage));
#elif ARCH(i686)
		asm volatile("fxsave %0" :: "m"(m_sse_storage));
#else
#error
#endif
	}

	void Thread::load_sse()
	{
#if ARCH(x86_64)
		asm volatile("fxrstor64 %0" :: "m"(m_sse_storage));
#elif ARCH(i686)
		asm volatile("fxrstor %0" :: "m"(m_sse_storage));
#else
#error
#endif
	}

}
