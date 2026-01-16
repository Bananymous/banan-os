#pragma once

#include <BAN/Atomic.h>
#include <BAN/ForwardList.h>

#include <kernel/API/SharedPage.h>
#include <kernel/Arch.h>
#include <kernel/GDT.h>
#include <kernel/IDT.h>
#include <kernel/InterruptStack.h>
#include <kernel/Memory/Types.h>
#include <kernel/ProcessorID.h>
#include <kernel/Scheduler.h>

namespace Kernel
{

	enum class InterruptState
	{
		Disabled,
		Enabled,
	};

#if ARCH(x86_64) || ARCH(i686)
	class Processor
	{
		BAN_NON_COPYABLE(Processor);
		BAN_NON_MOVABLE(Processor);

	public:
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
				struct
				{
					uintptr_t vaddr;
					size_t page_count;
					void* page_table;
				} flush_tlb;
				SchedulerQueue::Node* new_thread;
				SchedulerQueue::Node* unblock_thread;
				bool dummy;
			};
		};

	public:
		static Processor& create(ProcessorID id);
		static Processor& initialize();

		static ProcessorID current_id() { return read_gs_sized<ProcessorID>(offsetof(Processor, m_id)); }
		static uint8_t current_index()  { return read_gs_sized<uint8_t>(offsetof(Processor, m_index)); }
		static ProcessorID id_from_index(size_t index);

		static uint8_t count()        { return s_processor_count; }
		static bool is_smp_enabled()  { return s_is_smp_enabled; }
		static void set_smp_enabled() { s_is_smp_enabled = true; }
		static void wait_until_processors_ready();

		static void toggle_should_print_cpu_load() { s_should_print_cpu_load = !s_should_print_cpu_load; }
		static bool get_should_print_cpu_load() { return s_should_print_cpu_load; }

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
			if (is_smp_enabled())
				handle_smp_messages();
		}

		static uintptr_t current_stack_bottom() { return read_gs_sized<uintptr_t>(offsetof(Processor, m_stack)); }
		static uintptr_t current_stack_top()	{ return current_stack_bottom() + s_stack_size; }

		uintptr_t stack_bottom() const	{ return reinterpret_cast<uintptr_t>(m_stack); }
		uintptr_t stack_top() const		{ return stack_bottom() + s_stack_size; }

		static void set_thread_syscall_stack(vaddr_t vaddr) { write_gs_sized<vaddr_t>(offsetof(Processor, m_thread_syscall_stack), vaddr); }

		static GDT& gdt() { return *read_gs_sized<GDT*>(offsetof(Processor, m_gdt)); }
		static IDT& idt() { return *read_gs_sized<IDT*>(offsetof(Processor, m_idt)); }

		static void* get_current_page_table()					{ return read_gs_sized<void*>(offsetof(Processor, m_current_page_table)); }
		static void set_current_page_table(void* page_table)	{ write_gs_sized<void*>(offsetof(Processor, m_current_page_table), page_table); }

		static void yield();
		static Scheduler& scheduler() { return *read_gs_sized<Scheduler*>(offsetof(Processor, m_scheduler)); }

		static void initialize_tsc(uint8_t shift, uint64_t mult, uint64_t realtime_seconds);
		static void update_tsc();
		static uint64_t ns_since_boot_tsc();

		static Thread* get_current_sse_thread() { return read_gs_sized<Thread*>(offsetof(Processor, m_sse_thread)); };
		static void set_current_sse_thread(Thread* thread) { write_gs_sized<Thread*>(offsetof(Processor, m_sse_thread), thread); };

		static paddr_t shared_page_paddr() { return s_shared_page_paddr; }
		static volatile API::SharedPage& shared_page() { return *reinterpret_cast<API::SharedPage*>(s_shared_page_vaddr); }

		static void handle_ipi();

		static void handle_smp_messages();
		static void send_smp_message(ProcessorID, const SMPMessage&, bool send_ipi = true);
		static void broadcast_smp_message(const SMPMessage&);

		static void load_segments();
		static void load_fsbase();
		static void load_gsbase();

		static void disable_sse()
		{
			uintptr_t dummy;
#if ARCH(x86_64)
			asm volatile("movq %%cr0, %0; orq $0x08, %0; movq %0, %%cr0" : "=r"(dummy));
#elif ARCH(i686)
			asm volatile("movl %%cr0, %0; orl $0x08, %0; movl %0, %%cr0" : "=r"(dummy));
#endif
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
		static T read_gs_sized(uintptr_t offset) requires(sizeof(T) <= 8)
		{
#define __ASM_INPUT(operation) asm volatile(operation " %%gs:%a[offset], %[result]" : [result]"=r"(result) : [offset]"ir"(offset))
			T result;
			if constexpr(sizeof(T) == 8)
				__ASM_INPUT("movq");
			if constexpr(sizeof(T) == 4)
				__ASM_INPUT("movl");
			if constexpr(sizeof(T) == 2)
				__ASM_INPUT("movw");
			if constexpr(sizeof(T) == 1)
				__ASM_INPUT("movb");
			return result;
#undef __ASM_INPUT
		}

		template<typename T>
		static void write_gs_sized(uintptr_t offset, T value) requires(sizeof(T) <= 8)
		{
#define __ASM_INPUT(operation) asm volatile(operation " %[value], %%gs:%a[offset]" :: [value]"r"(value), [offset]"ir"(offset) : "memory")
			if constexpr(sizeof(T) == 8)
				__ASM_INPUT("movq");
			if constexpr(sizeof(T) == 4)
				__ASM_INPUT("movl");
			if constexpr(sizeof(T) == 2)
				__ASM_INPUT("movw");
			if constexpr(sizeof(T) == 1)
				__ASM_INPUT("movb");
#undef __ASM_INPUT
		}

	private:
		static ProcessorID s_bsp_id;
		static BAN::Atomic<uint8_t> s_processor_count;
		static BAN::Atomic<bool>    s_is_smp_enabled;
		static BAN::Atomic<bool>    s_should_print_cpu_load;
		static paddr_t              s_shared_page_paddr;
		static vaddr_t              s_shared_page_vaddr;

		ProcessorID m_id { 0 };
		uint8_t m_index { 0xFF };

		vaddr_t m_thread_syscall_stack;

		Thread* m_sse_thread { nullptr };

		static constexpr size_t s_stack_size { 4096 };
		void* m_stack { nullptr };

		GDT* m_gdt { nullptr };
		IDT* m_idt { nullptr };

		Scheduler* m_scheduler { nullptr };

		uint64_t m_start_ns { 0 };
		uint64_t m_idle_ns { 0 };
		uint64_t m_last_update_ns { 0 };
		uint64_t m_next_update_ns { 0 };

		BAN::Atomic<SMPMessage*> m_smp_pending { nullptr };
		BAN::Atomic<SMPMessage*> m_smp_free    { nullptr };
		SMPMessage* m_smp_message_storage { nullptr };

		void* m_current_page_table { nullptr };

		friend class BAN::Array<Processor, 0xFF>;
	};
#else
	#error
#endif

}
