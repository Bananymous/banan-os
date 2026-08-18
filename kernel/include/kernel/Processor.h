#pragma once

#include <BAN/Array.h>
#include <BAN/Atomic.h>
#include <BAN/NoCopyMove.h>
#include <BAN/Math.h>

#include <kernel/API/SharedPage.h>
#include <kernel/Arch.h>
#include <kernel/Memory/Types.h>
#include <kernel/ProcessorID.h>

namespace Kernel
{

	enum class InterruptState
	{
		Disabled,
		Enabled,
	};

	class GDT;
	class IDT;
	class Scheduler;
	class SchedulerQueueNode;
	class Thread;

#if ARCH(x86_64) || ARCH(i686)
	class Processor
	{
		BAN_NON_COPYABLE(Processor);
		BAN_NON_MOVABLE(Processor);

	public:
		struct TLBEntry
		{
			vaddr_t vaddr;
			size_t page_count;
			class PageTable* page_table;
		};

		struct SMPMessage
		{
			enum class Type
			{
				FlushTLB,
				NewThread,
				UnblockThread,
				UpdateTSC,
				StackTrace,
			};
			SMPMessage* next { nullptr };
			Type type;
			union
			{
				TLBEntry flush_tlb;
				SchedulerQueueNode* new_thread;
				SchedulerQueueNode* unblock_thread;
				bool dummy;
			};
		};

		struct LoadStats
		{
			uint64_t ns_idle;
			uint64_t ns_total;
		};

	public:
		static Processor& create(ProcessorID id);
		static Processor& initialize();

		void allocate_stack();

		static ProcessorID current_id() { return read_gs_sized<ProcessorID>(offsetof(Processor, m_id)); }
		static uint8_t current_index()  { return read_gs_sized<uint8_t>(offsetof(Processor, m_index)); }
		static ProcessorID id_from_index(size_t index);

		static uint8_t count()        { return s_processor_count; }
		static bool is_smp_enabled()  { return s_is_smp_enabled; }
		static void set_smp_enabled() { s_is_smp_enabled = true; }
		static void wait_until_processors_ready();

		static ProcessorID bsp_id() { return s_bsp_id; }
		static bool current_is_bsp() { return current_id() == bsp_id(); }

		static void set_interrupt_state(InterruptState state)
		{
			if (state == InterruptState::Enabled)
				asm volatile("sti");
			else
				asm volatile("cli");
		}

		static InterruptState get_interrupt_state()
		{
#if ARCH(x86_64)
			const auto flags = __builtin_ia32_readeflags_u64();
#elif ARCH(i686)
			const auto flags = __builtin_ia32_readeflags_u32();
#endif
			if (flags & (1 << 9))
				return InterruptState::Enabled;
			return InterruptState::Disabled;
		};

		static void pause()
		{
			__builtin_ia32_pause();
			if (is_smp_enabled() && !smp_messages_disabled())
				handle_smp_messages();
		}

		static bool smp_messages_disabled() { return read_gs_sized<bool>(offsetof(Processor, m_smp_messages_disabled)); }
		static void set_disable_smp_messages(bool disabled)
		{
			ASSERT(smp_messages_disabled() != disabled);
			write_gs_sized<bool>(offsetof(Processor, m_smp_messages_disabled), disabled);
		}

		vaddr_t stack_top_vaddr() const { return m_stack_vaddr + s_stack_size; }
		paddr_t stack_top_paddr() const { return m_stack_paddr + s_stack_size; }

		static void set_thread_syscall_stack(vaddr_t vaddr) { write_gs_sized<vaddr_t>(offsetof(Processor, m_thread_syscall_stack), vaddr); }

		static GDT& gdt() { return *read_gs_sized<GDT*>(offsetof(Processor, m_gdt)); }
		static IDT& idt() { return *read_gs_sized<IDT*>(offsetof(Processor, m_idt)); }

		static void* get_current_page_table()					{ return read_gs_sized<void*>(offsetof(Processor, m_current_page_table)); }
		static void set_current_page_table(void* page_table)	{ write_gs_sized<void*>(offsetof(Processor, m_current_page_table), page_table); }

		static LoadStats get_load_stats(size_t index);

		static void yield();
		static Scheduler& scheduler() { return *read_gs_sized<Scheduler*>(offsetof(Processor, m_scheduler)); }

		static void initialize_tsc(uint64_t realtime_seconds);
		static void update_tsc();
		static uint64_t ns_since_boot_tsc();

		static Thread* get_current_sse_thread() { return read_gs_sized<Thread*>(offsetof(Processor, m_sse_thread)); };
		static void set_current_sse_thread(Thread* thread) { write_gs_sized<Thread*>(offsetof(Processor, m_sse_thread), thread); };

		static paddr_t shared_page_paddr() { return s_shared_page_paddr; }
		static volatile API::SharedPage& shared_page() { return *reinterpret_cast<API::SharedPage*>(s_shared_page_vaddr); }

		static void handle_ipi();

		static void handle_smp_messages();
		static bool send_smp_message(ProcessorID, const SMPMessage&, bool send_ipi = true);
		static void broadcast_smp_message(const SMPMessage&);

		static void load_segments();
		static void load_fsbase();
		static void load_gsbase();

		static void disable_sse()
		{
			uintptr_t dummy;
			asm volatile("mov %%cr0, %0; or $0x08, %0; mov %0, %%cr0" : "=r"(dummy));
		}

		static void enable_sse()
		{
			asm volatile("clts");
		}

	private:
		Processor() = default;
		~Processor() { ASSERT_NOT_REACHED(); }

		static ProcessorID read_processor_id();

		static void initialize_smp();
		static void initialize_shared_page();

		static void dummy()
		{
#if ARCH(x86_64)
			static_assert(offsetof(Processor, m_thread_syscall_stack) == 8, "This is hardcoded in Syscall.S");
#endif
		}

		template<typename T>
		static T read_gs_sized(uintptr_t offset) requires(sizeof(T) <= 8 && BAN::Math::is_power_of_two(sizeof(T)))
		{
			T value;
			asm volatile("mov %%gs:%a[offset], %[value]" : [value]"=r"(value) : [offset]"ir"(offset));
			return value;
		}

		template<typename T>
		static void write_gs_sized(uintptr_t offset, T value) requires(sizeof(T) <= 8 && BAN::Math::is_power_of_two(sizeof(T)))
		{
			asm volatile("mov %[value], %%gs:%a[offset]" :: [value]"r"(value), [offset]"ir"(offset) : "memory");
		}

		void lock_tlb_lock();
		void unlock_tlb_lock();

	private:
		static ProcessorID s_bsp_id;
		static BAN::Atomic<uint8_t> s_processor_count;
		static BAN::Atomic<bool>    s_is_smp_enabled;
		static paddr_t              s_shared_page_paddr;
		static vaddr_t              s_shared_page_vaddr;

		ProcessorID m_id { 0 };
		uint8_t m_index { 0 };

		bool m_smp_messages_disabled { false };

		vaddr_t m_thread_syscall_stack;

		Thread* m_sse_thread { nullptr };

		static constexpr size_t s_stack_size { PAGE_SIZE };
		vaddr_t m_stack_vaddr { 0 };
		paddr_t m_stack_paddr { 0 };

		GDT* m_gdt { nullptr };
		IDT* m_idt { nullptr };

		Scheduler* m_scheduler { nullptr };

		BAN::Atomic<bool> m_load_stat_lock;
		uint64_t m_load_start_ns { 0 };
		LoadStats m_load_stats {};

		BAN::Atomic<SMPMessage*> m_smp_pending { nullptr };
		BAN::Atomic<SMPMessage*> m_smp_free    { nullptr };
		SMPMessage* m_smp_message_storage { nullptr };

		BAN::Atomic<bool> m_tlb_lock { false };
		size_t m_tlb_entry_count { 0 };
		BAN::Array<TLBEntry, 32> m_tlb_entries;
		bool m_tlb_global { false };

		void* m_current_page_table { nullptr };

		friend class BAN::Array<Processor, 0xFF>;
	};
#else
	#error
#endif

}
