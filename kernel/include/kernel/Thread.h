#pragma once

#include <BAN/NoCopyMove.h>
#include <BAN/Optional.h>
#include <BAN/RefPtr.h>
#include <BAN/UniqPtr.h>
#include <kernel/InterruptStack.h>
#include <kernel/Memory/VirtualRange.h>
#include <kernel/ThreadBlocker.h>

#include <LibELF/AuxiliaryVector.h>

#include <signal.h>
#include <sys/types.h>

namespace Kernel
{

	class MemoryBackedRegion;
	class Process;

	class Thread
	{
		BAN_NON_COPYABLE(Thread);
		BAN_NON_MOVABLE(Thread);

	public:
		using entry_t = void(*)(void*);

		enum class State
		{
			NotStarted,
			Executing,
			Terminated,
		};

		// FIXME: kernel stack does NOT have to be this big, but my recursive AML interpreter
		//        stack overflows on some machines with 8 page stack
		static constexpr size_t kernel_stack_size    { PAGE_SIZE * 16 };

		// TODO: userspace stack size is hard limited, maybe make this dynamic?
#if ARCH(x86_64)
		static constexpr size_t userspace_stack_size { 32 << 20 };
		static constexpr vaddr_t userspace_stack_base { 0x0000700000000000 };
#elif ARCH(i686)
		static constexpr size_t userspace_stack_size { 4 << 20 };
		static constexpr vaddr_t userspace_stack_base { 0xB0000000 };
#endif

	public:
		static BAN::ErrorOr<Thread*> create_kernel(entry_t, void*);
		static BAN::ErrorOr<Thread*> create_userspace(Process*, PageTable&, vaddr_t userspace_stack_vaddr, size_t userspace_stack_size, vaddr_t entry_point, vaddr_t stack_pointer);
		~Thread();

		BAN::ErrorOr<Thread*> clone(Process*, uintptr_t sp, uintptr_t ip);
		void setup_process_cleanup();

		// Returns true, if thread is going to trigger signal
		bool is_interrupted_by_signal(bool skip_stop_and_cont = false) const;

		// Returns true if handled signal had SA_RESTART
		bool handle_signal_if_interrupted();
		bool handle_signal(int signal, const siginfo_t&);
		void add_signal(int signal, const siginfo_t&);
		void set_suspend_signal_mask(uint64_t sigmask);

		static bool is_stopping_signal(int signal);
		static bool is_continuing_signal(int signal);
		static bool is_terminating_signal(int signal);
		static bool is_abnormal_terminating_signal(int signal);
		static bool is_realtime_signal(int signal);
		bool will_exit_because_of_signal() const;

		BAN::ErrorOr<void> sigaltstack(const stack_t* ss, stack_t* oss);

		// blocks current thread and returns either on unblock, eintr, spuriously or after timeout
		// if mutex is not nullptr, it will be atomically freed before blocking and automatically locked on wake
		BAN::ErrorOr<void> sleep_or_eintr_for_ns(uint64_t timeout_ns);
		BAN::ErrorOr<void> sleep_or_eintr_until_ns(uint64_t waketime_ns);
		BAN::ErrorOr<void> block_or_eintr_indefinite(ThreadBlocker& thread_blocker, BaseMutex* mutex);
		BAN::ErrorOr<void> block_or_eintr_or_timeout_ns(ThreadBlocker& thread_blocker, uint64_t timeout_ns, bool etimedout, BaseMutex* mutex);
		BAN::ErrorOr<void> block_or_eintr_or_waketime_ns(ThreadBlocker& thread_blocker, uint64_t wake_time_ns, bool etimedout, BaseMutex* mutex);

		BAN::ErrorOr<void> sleep_or_eintr_for_ms(uint64_t timeout_ms)
		{
			ASSERT(!BAN::Math::will_multiplication_overflow<uint64_t>(timeout_ms, 1'000'000));
			return sleep_or_eintr_for_ns(timeout_ms * 1'000'000);
		}
		BAN::ErrorOr<void> sleep_or_eintr_until_ms(uint64_t waketime_ms)
		{
			ASSERT(!BAN::Math::will_multiplication_overflow<uint64_t>(waketime_ms, 1'000'000));
			return sleep_or_eintr_until_ns(waketime_ms * 1'000'000);
		}
		BAN::ErrorOr<void> block_or_eintr_or_timeout_ms(ThreadBlocker& thread_blocker, uint64_t timeout_ms, bool etimedout, BaseMutex* mutex)
		{
			ASSERT(!BAN::Math::will_multiplication_overflow<uint64_t>(timeout_ms, 1'000'000));
			return block_or_eintr_or_timeout_ns(thread_blocker, timeout_ms * 1'000'000, etimedout, mutex);
		}
		BAN::ErrorOr<void> block_or_eintr_or_waketime_ms(ThreadBlocker& thread_blocker, uint64_t wake_time_ms, bool etimedout, BaseMutex* mutex)
		{
			ASSERT(!BAN::Math::will_multiplication_overflow<uint64_t>(wake_time_ms, 1'000'000));
			return block_or_eintr_or_waketime_ns(thread_blocker, wake_time_ms * 1'000'000, etimedout, mutex);
		}

		pid_t tid() const { return m_tid; }

		State state() const { return m_state; }

		vaddr_t kernel_stack_bottom() const	{ return m_kernel_stack->vaddr(); }
		vaddr_t kernel_stack_top() const	{ return m_kernel_stack->vaddr() + m_kernel_stack->size(); }
		VirtualRange& kernel_stack() { return *m_kernel_stack; }

		static Thread& current();
		static pid_t current_tid();

		void give_keep_alive_page_table(BAN::UniqPtr<PageTable>&& page_table) { m_keep_alive_page_table = BAN::move(page_table); }

		Process& process();
		const Process& process() const;
		bool has_process() const { return m_process; }

		void detach() { m_is_detached = true; }
		bool is_detached() const { return m_is_detached; }

		bool is_userspace() const { return m_is_userspace; }

		uint64_t cpu_time_total_ns() const;
		void cpu_time_ns(uint64_t& user_ns, uint64_t& system_ns) const;
		void set_cpu_time_start();
		void set_cpu_time_stop();

		void set_is_in_syscall(bool is_in_syscall);
		bool is_in_syscall() const { return m_is_in_syscall; }

		void update_processor_index_address();

		void set_fsbase(vaddr_t base) { m_fsbase = base; }
		vaddr_t get_fsbase() const { return m_fsbase; }
		void set_gsbase(vaddr_t base) { m_gsbase = base; }
		vaddr_t get_gsbase() const { return m_gsbase; }

		size_t virtual_page_count() const { return m_kernel_stack ? (m_kernel_stack->size() / PAGE_SIZE) : 0; }
		size_t physical_page_count() const { return virtual_page_count(); }

		YieldRegisters& yield_registers() { return m_yield_registers; }

		void save_sse();
		void load_sse();

		void add_spinlock() { m_spinlock_count++; }
		void remove_spinlock() { m_spinlock_count--; }
		bool has_spinlock() const { return !!m_spinlock_count; }

		void add_mutex() { m_mutex_count++; }
		void remove_mutex() { m_mutex_count--; }

	private:
		Thread(pid_t tid, Process*);

		static void on_exit_trampoline(Thread*);
		void on_exit();

		bool currently_on_alternate_stack() const;

		struct signal_handle_info_t
		{
			vaddr_t handler;
			vaddr_t stack_top;
			uint64_t restore_sigmask;
			bool has_sa_restart;
		};
		signal_handle_info_t remove_signal_and_get_info(int signal);
		void handle_signal_impl(int signal, const siginfo_t&, const signal_handle_info_t&);

	private:
		// NOTE: this is the first member to force it being last destructed
		//       {kernel,userspace}_stack has to be destroyed before page table
		BAN::UniqPtr<PageTable>    m_keep_alive_page_table;

		BAN::UniqPtr<VirtualRange> m_kernel_stack;
		const pid_t                m_tid                  { 0 };
		State                      m_state                { State::NotStarted };
		Process*                   m_process              { nullptr };
		bool                       m_is_userspace         { false };
		BAN::Atomic<bool>          m_is_detached          { false };
		bool                       m_delete_process       { false };

		vaddr_t                    m_userspace_stack_vaddr { 0 };
		size_t                     m_userspace_stack_size  { 0 };

		vaddr_t                    m_fsbase               { 0 };
		vaddr_t                    m_gsbase               { 0 };

		SchedulerQueue::Node*      m_scheduler_node       { nullptr };

		YieldRegisters             m_yield_registers      { };

		siginfo_t                  m_signal_infos[_SIGMAX + 1] { };
		uint64_t                   m_signal_pending_mask  { 0 };
		uint64_t                   m_signal_block_mask    { 0 };
		BAN::Optional<uint64_t>    m_signal_suspend_mask;
		SpinLock                   m_signal_lock;
		stack_t                    m_signal_alt_stack     { nullptr, 0, SS_DISABLE };
		static_assert(_SIGMAX < 64);

		mutable SpinLock           m_cpu_time_lock;
		uint64_t                   m_cpu_time_user_ns     { 0 };
		uint64_t                   m_cpu_time_system_ns   { 0 };
		uint64_t                   m_cpu_time_start_ns    { UINT64_MAX };
		BAN::Atomic<bool>          m_is_in_syscall        { false };

		BAN::Atomic<uint32_t>      m_spinlock_count       { 0 };
		BAN::Atomic<uint32_t>      m_mutex_count          { 0 };

		alignas(16) uint8_t m_sse_storage[512] {};

		friend class Process;
		friend class Scheduler;
	};

}
