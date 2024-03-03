#pragma once

#include <BAN/ForwardList.h>
#include <kernel/Arch.h>

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

	public:
		static Processor& create(ProcessorID id);

		static ProcessorID current_id()
		{
			uint16_t id;
			asm volatile("movw %%gs, %0" : "=rm"(id));
			return id;
		}

		static Processor& get(ProcessorID);

		static Processor& current() { return get(current_id()); }

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

		uintptr_t stack_bottom() const { return reinterpret_cast<uintptr_t>(m_stack); }
		uintptr_t stack_top() const { return stack_bottom() + m_stack_size; }

	private:
		Processor() = default;
		Processor(Processor&& other)
		{
			m_stack = other.m_stack;
			other.m_stack = nullptr;
		}
		~Processor();

	private:
		void* m_stack { nullptr };
		static constexpr size_t m_stack_size { 4096 };

		friend class BAN::Vector<Processor>;
	};
#else
	#error
#endif

}
