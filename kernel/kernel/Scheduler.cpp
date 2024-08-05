#include <BAN/Optional.h>
#include <BAN/Sort.h>
#include <kernel/InterruptController.h>
#include <kernel/Process.h>
#include <kernel/Scheduler.h>
#include <kernel/Thread.h>
#include <kernel/Timer/Timer.h>

#define DEBUG_SCHEDULER 0
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


	static BAN::Atomic<size_t> s_next_processor_index { 0 };


	void SchedulerQueue::add_thread_to_back(Node* node)
	{
		ASSERT(Processor::get_interrupt_state() == InterruptState::Disabled);
		node->next = nullptr;
		node->prev = m_tail;
		(m_tail ? m_tail->next : m_head) = node;
		m_tail = node;
	}

	void SchedulerQueue::add_thread_with_wake_time(Node* node)
	{
		ASSERT(Processor::get_interrupt_state() == InterruptState::Disabled);

		if (m_tail == nullptr || node->wake_time_ns >= m_tail->wake_time_ns)
			return add_thread_to_back(node);

		Node* next = m_head;
		Node* prev = nullptr;
		while (next && node->wake_time_ns > next->wake_time_ns)
		{
			prev = next;
			next = next->next;
		}

		node->next = next;
		node->prev = prev;
		(next ? next->prev : m_tail) = node;
		(prev ? prev->next : m_head) = node;
	}

	template<typename F>
	SchedulerQueue::Node* SchedulerQueue::remove_with_condition(F callback)
	{
		ASSERT(Processor::get_interrupt_state() == InterruptState::Disabled);

		for (Node* node = m_head; node; node = node->next)
		{
			if (!callback(node))
				continue;
			remove_node(node);
			return node;
		}

		return nullptr;
	}

	void SchedulerQueue::remove_node(Node* node)
	{
		(node->prev ? node->prev->next : m_head) = node->next;
		(node->next ? node->next->prev : m_tail) = node->prev;
		node->prev = nullptr;
		node->next = nullptr;
	}

	SchedulerQueue::Node* SchedulerQueue::front()
	{
		ASSERT(Processor::get_interrupt_state() == InterruptState::Disabled);
		ASSERT(!empty());
		return m_head;
	}

	SchedulerQueue::Node* SchedulerQueue::pop_front()
	{
		ASSERT(Processor::get_interrupt_state() == InterruptState::Disabled);
		if (empty())
			return nullptr;
		Node* result = m_head;
		m_head = m_head->next;
		(m_head ? m_head->prev : m_tail) = nullptr;
		result->next = nullptr;
		return result;
	}

	BAN::ErrorOr<Scheduler*> Scheduler::create()
	{
		auto* scheduler = new Scheduler();
		if (scheduler == nullptr)
			return BAN::Error::from_errno(ENOMEM);
		return scheduler;
	}

	BAN::ErrorOr<void> Scheduler::initialize()
	{
		m_idle_thread = TRY(Thread::create_kernel([](void*) { asm volatile("1: hlt; jmp 1b"); }, nullptr, nullptr));
		ASSERT(m_idle_thread);

		size_t processor_index = 0;
		for (; processor_index < Processor::count(); processor_index++)
			if (Processor::id_from_index(processor_index) == Processor::current_id())
				break;
		ASSERT(processor_index < Processor::count());

		// each CPU does load balance at different times. This calulates the offset to other CPUs
		m_last_load_balance_ns = s_load_balance_interval_ns * processor_index / Processor::count();
		m_idle_ns              = -m_last_load_balance_ns;

		s_schedulers_initialized++;
		while (s_schedulers_initialized < Processor::count())
			__builtin_ia32_pause();

		return {};
	}

	void Scheduler::add_current_to_most_loaded(SchedulerQueue* target_queue)
	{
		ASSERT(Processor::get_interrupt_state() == InterruptState::Disabled);

		bool has_current = false;
		for (auto& info : m_most_loaded_threads)
		{
			if (info.node == m_current)
			{
				info.queue = target_queue;
				has_current = true;
				break;
			}
		}

		if (!has_current)
		{
			size_t index = 0;
			for (; index < m_most_loaded_threads.size() - 1; index++)
				if (m_most_loaded_threads[index].node == nullptr)
					break;
			m_most_loaded_threads[index].queue = target_queue;
			m_most_loaded_threads[index].node  = m_current;
		}

		BAN::sort::sort(m_most_loaded_threads.begin(), m_most_loaded_threads.end(),
			[](const ThreadInfo& a, const ThreadInfo& b) -> bool
			{
				if (a.node == nullptr || b.node == nullptr)
					return a.node;
				return a.node->time_used_ns > b.node->time_used_ns;
			}
		);
	}

	void Scheduler::update_most_loaded_node_queue(SchedulerQueue::Node* node, SchedulerQueue* target_queue)
	{
		ASSERT(Processor::get_interrupt_state() == InterruptState::Disabled);

		for (auto& info : m_most_loaded_threads)
		{
			if (info.node == node)
			{
				info.queue = target_queue;
				break;
			}
		}
	}

	void Scheduler::remove_node_from_most_loaded(SchedulerQueue::Node* node)
	{
		ASSERT(Processor::get_interrupt_state() == InterruptState::Disabled);

		size_t i = 0;
		for (; i < m_most_loaded_threads.size(); i++)
			if (m_most_loaded_threads[i].node == node)
				break;

		for (; i < m_most_loaded_threads.size() - 1; i++)
			m_most_loaded_threads[i] = m_most_loaded_threads[i + 1];

		m_most_loaded_threads.back().node = nullptr;
		m_most_loaded_threads.back().queue = nullptr;
	}

	void Scheduler::reschedule(InterruptStack* interrupt_stack, InterruptRegisters* interrupt_registers)
	{
		ASSERT(Processor::get_interrupt_state() == InterruptState::Disabled);

		// If there are no other threads in run queue, reschedule can be no-op :)
		if (m_run_queue.empty() && (!m_current || !m_current->blocked) && current_thread().state() == Thread::State::Executing)
			return;

		if (m_current == nullptr)
			m_idle_ns += SystemTimer::get().ns_since_boot() - m_idle_start_ns;
		else
		{
			switch (m_current->thread->state())
			{
				case Thread::State::Terminated:
					remove_node_from_most_loaded(m_current);
					PageTable::kernel().load();
					delete m_current->thread;
					delete m_current;
					m_thread_count--;
					break;
				case Thread::State::Executing:
				{
					const uint64_t current_ns = SystemTimer::get().ns_since_boot();
					m_current->thread->interrupt_stack()     = *interrupt_stack;
					m_current->thread->interrupt_registers() = *interrupt_registers;
					m_current->time_used_ns += current_ns - m_current->last_start_ns;
					add_current_to_most_loaded(m_current->blocked ? &m_block_queue : &m_run_queue);
					if (!m_current->blocked)
						m_run_queue.add_thread_to_back(m_current);
					else
						m_block_queue.add_thread_with_wake_time(m_current);
					break;
				}
				case Thread::State::NotStarted:
					ASSERT(!m_current->blocked);
					m_current->time_used_ns = 0;
					remove_node_from_most_loaded(m_current);
					m_run_queue.add_thread_to_back(m_current);
					break;
			}
		}

		while ((m_current = m_run_queue.pop_front()))
		{
			if (m_current->thread->state() != Thread::State::Terminated)
				break;
			remove_node_from_most_loaded(m_current);
			PageTable::kernel().load();
			delete m_current->thread;
			delete m_current;
			m_thread_count--;
		}

		if (m_current == nullptr)
		{
			PageTable::kernel().load();
			*interrupt_stack       = m_idle_thread->interrupt_stack();
			*interrupt_registers   = m_idle_thread->interrupt_registers();
			m_idle_thread->m_state = Thread::State::Executing;
			m_idle_start_ns        = SystemTimer::get().ns_since_boot();
			return;
		}

		update_most_loaded_node_queue(m_current, nullptr);

		auto* thread = m_current->thread;

		auto& page_table = thread->has_process() ? thread->process().page_table() : PageTable::kernel();
		page_table.load();

		if (thread->state() == Thread::State::NotStarted)
			thread->m_state = Thread::State::Executing;

		Processor::gdt().set_tss_stack(thread->kernel_stack_top());
		*interrupt_stack     = thread->interrupt_stack();
		*interrupt_registers = thread->interrupt_registers();

		m_current->last_start_ns = SystemTimer::get().ns_since_boot();
	}

	void Scheduler::reschedule_if_idle()
	{
		ASSERT(Processor::get_interrupt_state() == InterruptState::Disabled);
		if (!m_current && !m_run_queue.empty())
			Processor::yield();
	}

	void Scheduler::timer_interrupt()
	{
		ASSERT(Processor::get_interrupt_state() == InterruptState::Disabled);

		if (Processor::is_smp_enabled())
			do_load_balancing();

		{
			const uint64_t current_ns = SystemTimer::get().ns_since_boot();
			while (!m_block_queue.empty() && current_ns >= m_block_queue.front()->wake_time_ns)
			{
				auto* node = m_block_queue.pop_front();
				if (node->blocker)
					node->blocker->remove_blocked_thread(node);
				node->blocked = false;
				update_most_loaded_node_queue(node, &m_run_queue);
				m_run_queue.add_thread_to_back(node);
			}
		}

		{
			const uint64_t current_ns = SystemTimer::get().ns_since_boot();
			if (current_ns >= m_last_reschedule_ns + s_reschedule_interval_ns)
			{
				m_last_reschedule_ns = current_ns;
				Processor::yield();
			}
		}
	}

	void Scheduler::unblock_thread(SchedulerQueue::Node* node)
	{
		auto state = Processor::get_interrupt_state();
		Processor::set_interrupt_state(InterruptState::Disabled);

		if (node->processor_id == Processor::current_id())
		{
			if (!node->blocked)
				return;
			if (node != m_current)
				m_block_queue.remove_node(node);
			if (node->blocker)
				node->blocker->remove_blocked_thread(node);
			node->blocked = false;
			if (node != m_current)
				m_run_queue.add_thread_to_back(node);
		}
		else
		{
			Processor::send_smp_message(node->processor_id, {
				.type = Processor::SMPMessage::Type::UnblockThread,
				.unblock_thread = node
			});
		}

		Processor::set_interrupt_state(state);
	}

	void Scheduler::add_thread(SchedulerQueue::Node* node)
	{
		auto state = Processor::get_interrupt_state();
		Processor::set_interrupt_state(InterruptState::Disabled);

		ASSERT(node->processor_id == Processor::current_id());

		if (!node->blocked)
			m_run_queue.add_thread_to_back(node);
		else
			m_block_queue.add_thread_with_wake_time(node);

		Processor::set_interrupt_state(state);
	}

	ProcessorID Scheduler::find_least_loaded_processor() const
	{
		ProcessorID least_loaded_id        = Processor::current_id();
		uint64_t    most_idle_ns           = m_idle_ns;
		uint32_t    least_max_load_threads = static_cast<uint32_t>(-1);
		for (uint8_t i = 0; i < Processor::count(); i++)
		{
			auto processor_id = Processor::id_from_index(i);
			if (processor_id == Processor::current_id())
				continue;
			const auto& info = s_processor_infos[i];
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

		if (m_current == nullptr)
		{
			m_idle_ns += current_ns - m_idle_start_ns;
			m_idle_start_ns = current_ns;
		}
		else
		{
			m_current->time_used_ns += current_ns - m_current->last_start_ns;
			m_current->last_start_ns = current_ns;
			add_current_to_most_loaded(nullptr);
		}

		if constexpr(DEBUG_SCHEDULER)
		{
			const uint64_t duration_ns = current_ns - m_last_load_balance_ns;
			const uint64_t processing_ns = duration_ns - m_idle_ns;

			{
				const uint64_t load_percent_x1000 = BAN::Math::div_round_up<uint64_t>(processing_ns * 100'000, duration_ns);
				dprintln("CPU {}: { 2}.{3}% ({} threads)", Processor::current_id(), load_percent_x1000 / 1000, load_percent_x1000 % 1000, m_thread_count);
			}

			if (m_current)
			{
				const char* name = "unknown";
				if (m_current->thread->has_process() && m_current->thread->process().is_userspace() && m_current->thread->process().userspace_info().argv)
					name = m_current->thread->process().userspace_info().argv[0];
				const uint64_t load_percent_x1000 = BAN::Math::div_round_up<uint64_t>(m_current->time_used_ns * 100'000, processing_ns);
				dprintln("  tid { 2}: { 3}.{3}% <{}> current", m_current->thread->tid(), load_percent_x1000 / 1000, load_percent_x1000 % 1000, name);
			}
			m_run_queue.remove_with_condition(
				[&](SchedulerQueue::Node* node)
				{
					const uint64_t load_percent_x1000 = BAN::Math::div_round_up<uint64_t>(node->time_used_ns * 100'000, processing_ns);
					dprintln("  tid { 2}: { 3}.{3}% active", node->thread->tid(), load_percent_x1000 / 1000, load_percent_x1000 % 1000);
					return false;
				}
			);
			m_block_queue.remove_with_condition(
				[&](SchedulerQueue::Node* node)
				{
					const uint64_t load_percent_x1000 = BAN::Math::div_round_up<uint64_t>(node->time_used_ns * 100'000, processing_ns);
					dprintln("  tid { 2}: { 3}.{3}% blocked", node->thread->tid(), load_percent_x1000 / 1000, load_percent_x1000 % 1000);
					return false;
				}
			);
		}

		if (!s_processor_info_time_lock.try_lock_interrupts_disabled())
		{
			dprintln_if(DEBUG_SCHEDULER, "Load balancing cannot keep up");
			return;
		}

		if (m_idle_ns == 0 && m_should_calculate_max_load_threads)
		{
			const auto& most_loaded_thread = m_most_loaded_threads.front();
			if (most_loaded_thread.node == nullptr || most_loaded_thread.node->time_used_ns == 0)
				s_processor_infos[Processor::current_id().as_u32()].max_load_threads = 0;
			else
			{
				const uint64_t duration_ns = current_ns - m_last_load_balance_ns;
				const uint64_t max_thread_load_x1000 = 1000 * m_most_loaded_threads.front().node->time_used_ns / duration_ns;
				const uint64_t max_load_thread_count = ((2000 / max_thread_load_x1000) + 1) / 2;
				s_processor_infos[Processor::current_id().as_u32()].max_load_threads = max_load_thread_count;
			}
		}

		constexpr auto absolute_difference_u64 = [](uint64_t a, uint64_t b) { return (a < b) ? (b - a) : (a - b); };

		for (size_t i = 1; i < m_most_loaded_threads.size(); i++)
		{
			auto& thread_info = m_most_loaded_threads[i];
			if (thread_info.node == nullptr)
				break;
			if (thread_info.node == m_current || thread_info.queue == nullptr)
				continue;
			// FIXME: allow load balancing with blocked threads, with this algorithm there is a race condition
			if (thread_info.node->blocked)
				continue;

			auto least_loaded_id = find_least_loaded_processor();
			if (least_loaded_id == Processor::current_id())
				break;

			auto& most_idle_info = s_processor_infos[least_loaded_id.as_u32()];
			auto& my_info = s_processor_infos[Processor::current_id().as_u32()];

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

					dprintln_if(DEBUG_SCHEDULER, "CPU {}: sending tid {} to CPU {} (max load)", Processor::current_id(), thread_info.node->thread->tid(), least_loaded_id);
				}
				else
				{
					my_info.max_load_threads        -= 1;
					most_idle_info.idle_time_ns      = 0;
					most_idle_info.max_load_threads  = 1;

					dprintln_if(DEBUG_SCHEDULER, "CPU {}: sending tid {} to CPU {}", Processor::current_id(), thread_info.node->thread->tid(), least_loaded_id);
				}
			}
			else
			{
				const uint64_t my_current_proc_ns    = s_load_balance_interval_ns - BAN::Math::min(s_load_balance_interval_ns, m_idle_ns);
				const uint64_t other_current_proc_ns = s_load_balance_interval_ns - BAN::Math::min(s_load_balance_interval_ns, most_idle_info.idle_time_ns);
				const uint64_t current_proc_diff_ns  = absolute_difference_u64(my_current_proc_ns, other_current_proc_ns);

				const uint64_t my_new_proc_ns    = my_current_proc_ns    - BAN::Math::min(thread_info.node->time_used_ns, my_current_proc_ns);
				const uint64_t other_new_proc_ns = other_current_proc_ns + thread_info.node->time_used_ns;
				const uint64_t new_proc_diff_ns  = absolute_difference_u64(my_new_proc_ns, other_new_proc_ns);

				// require 10% decrease between CPU loads to do send thread to other CPU
				if (new_proc_diff_ns >= current_proc_diff_ns || (100 * (current_proc_diff_ns - new_proc_diff_ns) / current_proc_diff_ns) < 10)
					continue;

				most_idle_info.idle_time_ns -= BAN::Math::min(thread_info.node->time_used_ns, most_idle_info.idle_time_ns);
				m_idle_ns                   += thread_info.node->time_used_ns;

				dprintln_if(DEBUG_SCHEDULER, "CPU {}: sending tid {} to CPU {}", Processor::current_id(), thread_info.node->thread->tid(), least_loaded_id);
			}

			thread_info.node->time_used_ns = 0;

			{
				auto& my_queue = (thread_info.queue == &m_run_queue) ? m_run_queue : m_block_queue;
				my_queue.remove_node(thread_info.node);
				m_thread_count--;
			}

			thread_info.node->processor_id = least_loaded_id;

			Processor::send_smp_message(least_loaded_id, {
				.type = Processor::SMPMessage::Type::NewThread,
				.new_thread = thread_info.node
			});

			thread_info.node = nullptr;
			thread_info.queue = nullptr;

			if (m_idle_ns == 0)
				break;
		}

		s_processor_infos[Processor::current_id().as_u32()].idle_time_ns = m_idle_ns;
		s_processor_info_time_lock.unlock(InterruptState::Disabled);

		if (m_current)
			m_current->time_used_ns = 0;
		for (auto& thread_info : m_most_loaded_threads)
			thread_info = {};
		m_run_queue  .remove_with_condition([&](SchedulerQueue::Node* node) { node->time_used_ns = 0; return false; });
		m_block_queue.remove_with_condition([&](SchedulerQueue::Node* node) { node->time_used_ns = 0; return false; });
		m_idle_ns = 0;

		m_should_calculate_max_load_threads = true;

		m_last_load_balance_ns += s_load_balance_interval_ns;
	}

	BAN::ErrorOr<void> Scheduler::add_thread(Thread* thread)
	{
		auto* new_node = new SchedulerQueue::Node(thread);
		if (new_node == nullptr)
			return BAN::Error::from_errno(ENOMEM);

		const size_t processor_index = s_next_processor_index++ % Processor::count();
		const auto processor_id = Processor::id_from_index(processor_index);

		new_node->processor_id = processor_id;
		thread->m_scheduler_node = new_node;

		if (processor_id == Processor::current_id())
			add_thread(new_node);
		else
		{
			Processor::send_smp_message(processor_id, {
				.type = Processor::SMPMessage::Type::NewThread,
				.new_thread = new_node
			});
		}

		return {};
	}

	void Scheduler::block_current_thread(ThreadBlocker* blocker, uint64_t wake_time_ns)
	{
		auto state = Processor::get_interrupt_state();
		Processor::set_interrupt_state(InterruptState::Disabled);

		ASSERT(!m_current->blocked);

		m_current->blocked = true;
		m_current->wake_time_ns = wake_time_ns;
		if (blocker)
			blocker->add_thread_to_block_queue(m_current);
		Processor::yield();

		Processor::set_interrupt_state(state);
	}

	void Scheduler::unblock_thread(Thread* thread)
	{
		auto state = Processor::get_interrupt_state();
		Processor::set_interrupt_state(InterruptState::Disabled);
		unblock_thread(thread->m_scheduler_node);
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
