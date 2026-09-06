#include <BAN/Optional.h>
#include <BAN/ScopeGuard.h>
#include <BAN/Sort.h>
#include <kernel/APIC.h>
#include <kernel/GDT.h>
#include <kernel/InterruptController.h>
#include <kernel/Lock/Mutex.h>
#include <kernel/Process.h>
#include <kernel/Scheduler.h>
#include <kernel/Thread.h>
#include <kernel/Timer/Timer.h>

#define SCHEDULER_ASSERT 1

#if SCHEDULER_ASSERT == 0
#undef ASSERT
#define ASSERT(...)
#endif

namespace Kernel
{

	static constexpr uint64_t s_reschedule_interval_ns   =    10'000'000;
	static constexpr uint64_t s_load_balance_interval_ns = 1'000'000'000;

	static BAN::Atomic<uint8_t> s_schedulers_initialized { 0 };

	struct ProcessorInfo
	{
		uint64_t idle_time_ns     { s_load_balance_interval_ns };
		uint32_t max_load_threads { 0 };
	};

	static SpinLock                        s_processor_info_time_lock;
	static BAN::Array<ProcessorInfo, 0xFF> s_processor_infos;

	BAN::ErrorOr<Scheduler*> Scheduler::create()
	{
		auto* scheduler = new Scheduler();
		if (scheduler == nullptr)
			return BAN::Error::from_errno(ENOMEM);
		return scheduler;
	}

	BAN::ErrorOr<void> Scheduler::initialize()
	{
		m_idle_thread = TRY(Thread::create_kernel([](void*) { asm volatile("1: hlt; jmp 1b"); }, nullptr));
		ASSERT(m_idle_thread);

		// each CPU does load balance at different times. This calulates the offset to other CPUs
		m_last_load_balance_ns = s_load_balance_interval_ns * Processor::current_index() / Processor::count();
		m_idle_ns              = m_last_load_balance_ns;

		s_schedulers_initialized++;
		while (s_schedulers_initialized < Processor::count())
			__builtin_ia32_pause();

		if (Processor::count() > 1)
			Processor::set_smp_enabled();

		return {};
	}

	void Scheduler::add_current_to_most_loaded()
	{
		ASSERT(Processor::get_interrupt_state() == InterruptState::Disabled);

		for (const auto* thread : m_most_loaded_threads)
			if (thread == m_current)
				return;

		for (size_t i = 0; i < m_most_loaded_threads.size(); i++)
		{
			if (m_most_loaded_threads[i] != nullptr)
				continue;
			m_most_loaded_threads[i] = m_current;
			return;
		}

		size_t min_index { 0 };
		for (size_t i = 1; i < m_most_loaded_threads.size(); i++)
			if (m_most_loaded_threads[i]->time_used_ns < m_most_loaded_threads[min_index]->time_used_ns)
				min_index = i;
		if (m_current->time_used_ns > m_most_loaded_threads[min_index]->time_used_ns)
			m_most_loaded_threads[min_index] = m_current;
	}

	void Scheduler::remove_current_from_most_loaded()
	{
		ASSERT(Processor::get_interrupt_state() == InterruptState::Disabled);

		for (size_t i = 0; i < m_most_loaded_threads.size(); i++)
		{
			if (m_most_loaded_threads[i] != m_current)
				continue;
			m_most_loaded_threads[i] = nullptr;
			return;
		}
	}

	void Scheduler::reschedule(YieldRegisters* yield_registers)
	{
		ASSERT(Processor::get_interrupt_state() == InterruptState::Disabled);

		// If there are no other threads in run queue, reschedule can be no-op :)
		if (m_run_list.empty() && (!m_current || !m_current->blocked) && current_thread().state() == Thread::State::Executing)
			return;

		if (m_current == nullptr)
			m_idle_ns += SystemTimer::get().ns_since_boot() - m_idle_start_ns;
		else
		{
			m_current->thread->set_cpu_time_stop();

			switch (m_current->thread->state())
			{
				case Thread::State::Terminated:
				{
					remove_current_from_most_loaded();
					if (&PageTable::current() != &PageTable::kernel())
						PageTable::kernel().load();
					auto* thread = m_current->thread;
					m_current = nullptr;
					delete thread;
					m_thread_count--;
					break;
				}
				case Thread::State::Executing:
					m_current->thread->yield_registers() = *yield_registers;
					m_current->time_used_ns += SystemTimer::get().ns_since_boot() - m_current->last_start_ns;
					add_current_to_most_loaded();
					if (!m_current->blocked)
						m_run_list.push(m_current);
					else
					{
						m_block_list.push(m_current);
						if (m_block_list.front() == m_current)
							update_wake_up_deadline();
					}
					break;
				case Thread::State::NotStarted:
					ASSERT(!m_current->blocked);
					m_current->time_used_ns = 0;
					remove_current_from_most_loaded();
					m_run_list.push(m_current);
					break;
			}
		}

		m_current = m_run_list.pop_front();

		if (m_current == nullptr)
		{
			if (&PageTable::current() != &PageTable::kernel())
				PageTable::kernel().load();
			*yield_registers = m_idle_thread->yield_registers();
			m_idle_thread->m_state = Thread::State::Executing;
			m_idle_start_ns        = SystemTimer::get().ns_since_boot();
			return;
		}

		ASSERT(m_current->thread->state() != Thread::State::Terminated);
		ASSERT(!m_current->blocked);

		auto* thread = m_current->thread;

		auto& page_table = thread->has_process() ? thread->process().page_table() : PageTable::kernel();
		if (&PageTable::current() != &page_table)
			page_table.load();

		if (thread->state() == Thread::State::NotStarted)
			thread->m_state = Thread::State::Executing;

		thread->set_cpu_time_start();

		if (thread->is_userspace())
		{
			const vaddr_t kernel_stack_top = thread->kernel_stack_top();
			Processor::gdt().set_tss_stack(kernel_stack_top);
			Processor::set_thread_syscall_stack(kernel_stack_top);
			Processor::load_segments();
		}

		(Processor::current_sse_thread() == thread)
			? Processor::enable_sse()
			: Processor::disable_sse();

		*yield_registers = thread->yield_registers();

		m_current->last_start_ns = SystemTimer::get().ns_since_boot();
	}

	void Scheduler::wake_up_sleeping_threads()
	{
		ASSERT(Processor::get_interrupt_state() == InterruptState::Disabled);

		const uint64_t current_ns = SystemTimer::get().ns_since_boot();
		while (!m_block_list.empty() && current_ns >= m_block_list.front()->wake_time_ns)
			unblock_thread(m_block_list.front()->thread);
	}

	void Scheduler::update_wake_up_deadline()
	{
		ASSERT(Processor::get_interrupt_state() == InterruptState::Disabled);

		auto& interrupt_controller = InterruptController::get();

		// TODO: support timer deadlines on non-apic timers and abstract it :)
		if (!interrupt_controller.is_using_apic())
			return;

		uint64_t deadline_ns = m_next_reschedule_ns;
		if (!m_block_list.empty())
			deadline_ns = BAN::Math::min(deadline_ns, m_block_list.front()->wake_time_ns);
		if (Processor::is_smp_enabled())
			deadline_ns = BAN::Math::min(deadline_ns, m_last_load_balance_ns + s_load_balance_interval_ns);

		static_cast<APIC&>(interrupt_controller).set_timer_dealine(deadline_ns);
	}

	void Scheduler::reschedule_if_needed()
	{
		ASSERT(Processor::get_interrupt_state() == InterruptState::Disabled);

		if ((is_idle() && !m_run_list.empty()) || m_has_pending_reschedule)
		{
			m_has_pending_reschedule = false;
			Processor::yield();
		}
	}

	extern "C" void scheduler_on_yield_trampoline(YieldRegisters* yield_registers)
	{
		Processor::set_disable_smp_messages(true);
		Processor::scheduler().on_yield(yield_registers);
		Processor::set_disable_smp_messages(false);
	}

	void Scheduler::on_yield(YieldRegisters* yield_registers)
	{
		Processor::update_load_stats(is_idle());

		reschedule(yield_registers);
		m_next_reschedule_ns = !is_idle()
			? SystemTimer::get().ns_since_boot() + s_reschedule_interval_ns
			: BAN::numeric_limits<uint64_t>::max();
		update_wake_up_deadline();
	}

	void Scheduler::on_timer_interrupt()
	{
		ASSERT(Processor::get_interrupt_state() == InterruptState::Disabled);

		Processor::update_load_stats(is_idle());

		if (Processor::is_smp_enabled())
			do_load_balancing();

		wake_up_sleeping_threads();

		// NOTE: yield will update the timer deadline, but if we do not yield make sure
		//       we set the next deadline or we won't get another timer interrupt
		if (is_idle() || SystemTimer::get().ns_since_boot() >= m_next_reschedule_ns)
			m_has_pending_reschedule = true;
		else
			update_wake_up_deadline();
	}

	ProcessorID Scheduler::find_least_loaded_processor() const
	{
		ProcessorID least_loaded_id        = Processor::current_id();
		uint64_t    most_idle_ns           = m_idle_ns;
		uint32_t    least_max_load_threads = static_cast<uint32_t>(-1);
		for (uint8_t i = 0; i < Processor::count(); i++)
		{
			const auto processor_id = Processor::id_from_index(i);
			if (processor_id == Processor::current_id())
				continue;
			const auto& info = s_processor_infos[processor_id.as_u32()];
			if (info.idle_time_ns < most_idle_ns || info.max_load_threads > least_max_load_threads)
				continue;
			least_loaded_id        = processor_id;
			most_idle_ns           = info.idle_time_ns;
			least_max_load_threads = info.max_load_threads;
		}
		return least_loaded_id;
	}

	void Scheduler::do_load_balancing()
	{
		ASSERT(Processor::get_interrupt_state() == InterruptState::Disabled);

		const uint64_t current_ns = SystemTimer::get().ns_since_boot();
		if (current_ns < m_last_load_balance_ns + s_load_balance_interval_ns)
			return;

		Processor::set_disable_smp_messages(true);
		BAN::ScopeGuard _([] {
			Processor::set_disable_smp_messages(false);
		});

		const auto current_id = Processor::current_id();

		if (m_current == nullptr)
		{
			m_idle_ns += current_ns - m_idle_start_ns;
			m_idle_start_ns = current_ns;
		}
		else
		{
			m_current->time_used_ns += current_ns - m_current->last_start_ns;
			m_current->last_start_ns = current_ns;
			add_current_to_most_loaded();
		}

		if constexpr(DEBUG_SCHEDULER)
		{
			const uint64_t duration_ns = current_ns - m_last_load_balance_ns;
			const uint64_t processing_ns = duration_ns - m_idle_ns;

			SpinLockGuard _(Debug::s_debug_lock);

			{
				const uint64_t load_percent_x1000 = BAN::Math::div_round_up<uint64_t>(processing_ns * 100'000, duration_ns);
				dprintln("CPU {}: { 2}.{3}% ({} threads)", current_id, load_percent_x1000 / 1000, load_percent_x1000 % 1000, m_thread_count);
			}

			if (m_current)
			{
				const char* name = "<unknown>";
				if (m_current->thread->has_process() && *m_current->thread->process().name())
					name = m_current->thread->process().name();
				const uint64_t load_percent_x1000 = BAN::Math::div_round_up<uint64_t>(m_current->time_used_ns * 100'000, processing_ns);
				dprintln("  tid { 2}: { 3}.{3}% current {}", m_current->thread->tid(), load_percent_x1000 / 1000, load_percent_x1000 % 1000, name);
			}

			m_run_list.walk([](const SchedulerThreadNode* node, void* arg) {
				const char* name = "<unknown>";
				if (node->thread->has_process() && *node->thread->process().name())
					name = node->thread->process().name();
				const uint64_t processing_ns = *static_cast<const uint64_t*>(arg);
				const uint64_t load_percent_x1000 = BAN::Math::div_round_up<uint64_t>(node->time_used_ns * 100'000, processing_ns);
				dprintln("  tid { 2}: { 3}.{3}% active {}", node->thread->tid(), load_percent_x1000 / 1000, load_percent_x1000 % 1000, name);
			}, const_cast<uint64_t*>(&processing_ns));

			m_block_list.walk([](const SchedulerThreadNode* node, void* arg) {
				const char* name = "<unknown>";
				if (node->thread->has_process() && *node->thread->process().name())
					name = node->thread->process().name();
				const uint64_t processing_ns = *static_cast<const uint64_t*>(arg);
				const uint64_t load_percent_x1000 = BAN::Math::div_round_up<uint64_t>(node->time_used_ns * 100'000, processing_ns);
				dprintln("  tid { 2}: { 3}.{3}% blocked {}", node->thread->tid(), load_percent_x1000 / 1000, load_percent_x1000 % 1000, name);
			}, const_cast<uint64_t*>(&processing_ns));
		}

		BAN::sort::sort(m_most_loaded_threads.begin(), m_most_loaded_threads.end(), [](auto* a, auto* b) {
			if (a == nullptr || b == nullptr)
				return a != nullptr;
			return a->time_used_ns > b->time_used_ns;
		});

		if (!s_processor_info_time_lock.try_lock_interrupts_disabled())
		{
			dprintln_if(DEBUG_SCHEDULER, "Load balancing cannot keep up");
			return;
		}

		if (m_idle_ns == 0 && m_should_calculate_max_load_threads)
		{
			const auto& most_loaded_thread = m_most_loaded_threads.front();
			if (most_loaded_thread == nullptr || most_loaded_thread->time_used_ns == 0)
				s_processor_infos[current_id.as_u32()].max_load_threads = 0;
			else
			{
				const uint64_t duration_ns = current_ns - m_last_load_balance_ns;
				const uint64_t max_thread_load_x1000 = 1000 * most_loaded_thread->time_used_ns / duration_ns;
				if (max_thread_load_x1000 == 0)
					s_processor_infos[current_id.as_u32()].max_load_threads = 0;
				else
				{
					const uint64_t max_load_thread_count = ((2000 / max_thread_load_x1000) + 1) / 2;
					s_processor_infos[current_id.as_u32()].max_load_threads = max_load_thread_count;
				}
			}
		}

		constexpr auto absolute_difference_u64 = [](uint64_t a, uint64_t b) { return (a < b) ? (b - a) : (a - b); };

		for (size_t i = 1; i < m_most_loaded_threads.size(); i++)
		{
			auto* heavy_thread = m_most_loaded_threads[i];
			if (heavy_thread == nullptr)
				break;
			ASSERT(heavy_thread->processor_id == current_id);
			if (heavy_thread == m_current)
				continue;

			const auto least_loaded_id = find_least_loaded_processor();
			if (least_loaded_id == current_id)
				break;

			auto& most_idle_info = s_processor_infos[least_loaded_id.as_u32()];
			auto& my_info = s_processor_infos[current_id.as_u32()];

			if (m_idle_ns == 0)
			{
				if (my_info.max_load_threads == 0)
					break;

				if (most_idle_info.idle_time_ns == 0)
				{
					if (most_idle_info.max_load_threads + 1 > my_info.max_load_threads - 1)
						break;

					my_info.max_load_threads        -= 1;
					most_idle_info.max_load_threads += 1;

					dprintln_if(DEBUG_SCHEDULER, "CPU {}: sending tid {} to CPU {} (max load)", current_id, heavy_thread->thread->tid(), least_loaded_id);
				}
				else
				{
					my_info.max_load_threads        -= 1;
					most_idle_info.idle_time_ns      = 0;
					most_idle_info.max_load_threads  = 1;

					dprintln_if(DEBUG_SCHEDULER, "CPU {}: sending tid {} to CPU {}", current_id, heavy_thread->thread->tid(), least_loaded_id);
				}
			}
			else
			{
				const uint64_t my_current_proc_ns    = s_load_balance_interval_ns - BAN::Math::min(s_load_balance_interval_ns, m_idle_ns);
				const uint64_t other_current_proc_ns = s_load_balance_interval_ns - BAN::Math::min(s_load_balance_interval_ns, most_idle_info.idle_time_ns);
				const uint64_t current_proc_diff_ns  = absolute_difference_u64(my_current_proc_ns, other_current_proc_ns);

				const uint64_t my_new_proc_ns    = my_current_proc_ns    - BAN::Math::min(heavy_thread->time_used_ns, my_current_proc_ns);
				const uint64_t other_new_proc_ns = other_current_proc_ns + heavy_thread->time_used_ns;
				const uint64_t new_proc_diff_ns  = absolute_difference_u64(my_new_proc_ns, other_new_proc_ns);

				// require 10% decrease between CPU loads to do send thread to other CPU
				if (new_proc_diff_ns >= current_proc_diff_ns || (100 * (current_proc_diff_ns - new_proc_diff_ns) / current_proc_diff_ns) < 10)
					continue;

				most_idle_info.idle_time_ns -= BAN::Math::min(heavy_thread->time_used_ns, most_idle_info.idle_time_ns);
				m_idle_ns                   += heavy_thread->time_used_ns;

				dprintln_if(DEBUG_SCHEDULER, "CPU {}: sending tid {} to CPU {}", current_id, heavy_thread->thread->tid(), least_loaded_id);
			}

			if (auto* thread = heavy_thread->thread; thread == Processor::current_sse_thread())
			{
				Processor::enable_sse();
				Processor::save_sse_state(*thread);
				Processor::reset_sse_thread();
				Processor::disable_sse();
			}

			heavy_thread->time_used_ns = 0;

			if (!heavy_thread->blocked)
				m_run_list.pop(heavy_thread);
			else
			{
				// NOTE: we spuriously wakeup threads on load balance as there is a race between
				//       changing owning processor and sending the SMP message
				m_block_list.pop(heavy_thread);
				if (auto* blocker = heavy_thread->blocker.load())
					blocker->remove_thread_from_block_queue(heavy_thread);
				heavy_thread->blocked = false;
			}

			m_thread_count--;

			heavy_thread->processor_id = least_loaded_id;
			Processor::send_smp_message(least_loaded_id, {
				.type = Processor::SMPMessage::Type::NewThread,
				.new_thread = heavy_thread->thread
			});

			m_most_loaded_threads[i] = nullptr;

			if (m_idle_ns == 0)
				break;
		}

		s_processor_infos[current_id.as_u32()].idle_time_ns = m_idle_ns;
		s_processor_info_time_lock.unlock(InterruptState::Disabled);

		if (m_current)
			m_current->time_used_ns = 0;
		for (auto*& thread : m_most_loaded_threads)
			thread = nullptr;
		m_run_list  .walk([](const auto* node, void*) { const_cast<SchedulerThreadNode*>(node)->time_used_ns = 0; }, nullptr);
		m_block_list.walk([](const auto* node, void*) { const_cast<SchedulerThreadNode*>(node)->time_used_ns = 0; }, nullptr);
		m_idle_ns = 0;

		m_should_calculate_max_load_threads = true;

		m_last_load_balance_ns += s_load_balance_interval_ns;
	}

	void Scheduler::bind_thread_to_processor(Thread* thread, ProcessorID processor_id)
	{
		ASSERT(processor_id != PROCESSOR_NONE);
		ASSERT(thread->m_scheduler_node.processor_id == PROCESSOR_NONE);
		thread->m_scheduler_node.processor_id = processor_id;
	}

	void Scheduler::add_thread(Thread* thread)
	{
		ASSERT(thread);

		if (thread->m_scheduler_node.processor_id == PROCESSOR_NONE)
		{
			static BAN::Atomic<size_t> s_next_processor_index { 0 };
			const size_t processor_index = s_next_processor_index++ % Processor::count();
			const auto processor_id = Processor::id_from_index(processor_index);
			bind_thread_to_processor(thread, processor_id);
		}

		if (const auto proc_id = thread->m_scheduler_node.processor_id; proc_id != Processor::current_id())
		{
			Processor::send_smp_message(proc_id, {
				.type = Processor::SMPMessage::Type::NewThread,
				.new_thread = thread
			});
			return;
		}

		const auto state = Processor::get_interrupt_state();
		Processor::set_interrupt_state(InterruptState::Disabled);

		ASSERT(!thread->m_scheduler_node.blocked);
		m_run_list.push(&thread->m_scheduler_node);

		if (thread->is_userspace() && thread->has_process())
			thread->update_processor_index_address();

		m_thread_count++;

		Processor::set_interrupt_state(state);
	}

	void Scheduler::block_current_thread(ThreadBlocker* blocker, uint64_t wake_time_ns, BaseMutex* mutex)
	{
		if (SystemTimer::get().ns_since_boot() >= wake_time_ns)
			return;

		const auto state = Processor::get_interrupt_state();
		Processor::set_interrupt_state(InterruptState::Disabled);

		ASSERT(m_current->processor_id == Processor::current_id());
		ASSERT(!m_current->blocked);

		Processor::set_disable_smp_messages(true);

		m_current->blocked = true;
		m_current->wake_time_ns = wake_time_ns;

		if (blocker != nullptr)
			blocker->add_thread_to_block_queue(m_current);

		uint32_t lock_depth = 0;
		if (mutex != nullptr)
		{
			ASSERT(mutex->is_locked_by_current_thread());
			lock_depth = mutex->lock_depth();
		}

		for (uint32_t i = 0; i < lock_depth; i++)
			mutex->unlock();

		Processor::set_disable_smp_messages(false);

		Processor::yield();

		// NOTE: we cannot touch `this` after yield as we may have been moved to another CPU
		//       and thus another Scheduler instance

		Processor::set_interrupt_state(state);

		for (uint32_t i = 0; i < lock_depth; i++)
			mutex->lock();
	}

	void Scheduler::unblock_thread(Thread* thread)
	{
		const auto state = Processor::get_interrupt_state();
		Processor::set_interrupt_state(InterruptState::Disabled);

		if (const auto proc_id = thread->m_scheduler_node.processor_id; proc_id != Processor::current_id())
		{
			Processor::send_smp_message(proc_id, {
				.type = Processor::SMPMessage::Type::UnblockThread,
				.unblock_thread = thread
			});
			return Processor::set_interrupt_state(state);
		}

		if (!thread->m_scheduler_node.blocked)
			return Processor::set_interrupt_state(state);

		Processor::set_disable_smp_messages(true);

		m_block_list.pop(&thread->m_scheduler_node);
		if (auto* blocker = thread->m_scheduler_node.blocker.load())
			blocker->remove_thread_from_block_queue(&thread->m_scheduler_node);
		thread->m_scheduler_node.blocked = false;

		m_run_list.push(&thread->m_scheduler_node);

		Processor::set_disable_smp_messages(false);

		Processor::set_interrupt_state(state);
	}

	Thread& Scheduler::current_thread()
	{
		if (m_current)
			return *m_current->thread;
		return *m_idle_thread;
	}

	Thread& Scheduler::idle_thread()
	{
		return *m_idle_thread;
	}

	pid_t Scheduler::current_tid() const
	{
		return m_current ? m_current->thread->tid() : 0;
	}

	bool Scheduler::is_idle() const
	{
		return m_current == nullptr;
	}

}
