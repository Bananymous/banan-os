#pragma once

#include <BAN/ForwardList.h>

#include <kernel/Arch.h>
#include <kernel/GDT.h>
#include <kernel/IDT.h>

namespace Kernel
{

	enum class InterruptState
	{
		Disabled,
		Enabled,
	};

	using ProcessorID = uint8_t;
	constexpr ProcessorID PROCESSOR_NONE = 0xFF;

#if ARCH(x86_64)
	class Processor
	{
		BAN_NON_COPYABLE(Processor);
		BAN_NON_MOVABLE(Processor);

	public:
		static Processor& create(ProcessorID id);
		static Processor& initialize();

		static ProcessorID current_id() { return read_gs_sized<ProcessorID>(offsetof(Processor, m_id)); }

		static ProcessorID bsb_id() { return s_bsb_id; }
		static bool current_is_bsb() { return current_id() == bsb_id(); }

		static void set_interrupt_state(InterruptState state)
		{
			if (state == InterruptState::Enabled)
				asm volatile("sti");
			else
				asm volatile("cli");
		}

		static InterruptState get_interrupt_state()
		{
			uintptr_t flags;
			asm volatile("pushf; pop %0" : "=rm"(flags));
			if (flags & (1 << 9))
				return InterruptState::Enabled;
			return InterruptState::Disabled;
		};

		static uintptr_t current_stack_bottom() { return reinterpret_cast<uintptr_t>(read_gs_ptr(offsetof(Processor, m_stack))); }
		static uintptr_t current_stack_top()	{ return current_stack_bottom() + s_stack_size; }

		uintptr_t stack_bottom() const	{ return reinterpret_cast<uintptr_t>(m_stack); }
		uintptr_t stack_top() const		{ return stack_bottom() + s_stack_size; }

		static GDT& gdt() { return *reinterpret_cast<GDT*>(read_gs_ptr(offsetof(Processor, m_gdt))); }
		static IDT& idt() { return *reinterpret_cast<IDT*>(read_gs_ptr(offsetof(Processor, m_idt))); }

		static void* get_current_page_table()					{ return read_gs_ptr(offsetof(Processor, m_current_page_table)); }
		static void set_current_page_table(void* page_table)	{ write_gs_ptr(offsetof(Processor, m_current_page_table), page_table); }

	private:
		Processor() = default;
		~Processor() { ASSERT_NOT_REACHED(); }

		template<typename T>
		static T read_gs_sized(uintptr_t offset) requires(sizeof(T) <= 8)
		{
#define __ASM_INPUT(operation) operation " %%gs:(%[offset]), %[result]" : [result]"=rm"(result) : [offset]"rm"(offset)
			T result;
			if constexpr(sizeof(T) == 8)
				asm volatile(__ASM_INPUT("movq"));
			if constexpr(sizeof(T) == 4)
				asm volatile(__ASM_INPUT("movl"));
			if constexpr(sizeof(T) == 2)
				asm volatile(__ASM_INPUT("movw"));
			if constexpr(sizeof(T) == 1)
				asm volatile(__ASM_INPUT("movb"));
			return result;
#undef __ASM_INPUT
		}

		template<typename T>
		static void write_gs_sized(uintptr_t offset, T value) requires(sizeof(T) <= 8)
		{
#define __ASM_INPUT(operation) operation " %[value], %%gs:(%[offset])" :: [value]"rm"(value), [offset]"rm"(offset) : "memory"
			if constexpr(sizeof(T) == 8)
				asm volatile(__ASM_INPUT("movq"));
			if constexpr(sizeof(T) == 4)
				asm volatile(__ASM_INPUT("movl"));
			if constexpr(sizeof(T) == 2)
				asm volatile(__ASM_INPUT("movw"));
			if constexpr(sizeof(T) == 1)
				asm volatile(__ASM_INPUT("movb"));
#undef __ASM_INPUT
		}

		static void* read_gs_ptr(uintptr_t offset) { return read_gs_sized<void*>(offset); }
		static void write_gs_ptr(uintptr_t offset, void* value) { write_gs_sized<void*>(offset, value); }

	private:
		static ProcessorID s_bsb_id;

		ProcessorID m_id { PROCESSOR_NONE };

		static constexpr size_t s_stack_size { 4096 };
		void* m_stack { nullptr };

		GDT* m_gdt { nullptr };
		IDT* m_idt { nullptr };

		void* m_current_page_table { nullptr };

		friend class BAN::Array<Processor, 0xFF>;
	};
#else
	#error
#endif

}
