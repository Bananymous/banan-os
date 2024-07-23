#pragma once

#include <BAN/Formatter.h>

namespace Kernel
{

	class ProcessorID
	{
	public:
		using value_type = uint32_t;

	public:
		ProcessorID() = default;

		uint32_t as_u32() const { return m_id; }
		bool operator==(ProcessorID other) const { return m_id == other.m_id; }

	private:
		explicit ProcessorID(uint32_t id) : m_id(id) {}

	private:
		uint32_t m_id = static_cast<uint32_t>(-1);

		friend class Processor;
		friend class APIC;
	};

	inline constexpr ProcessorID PROCESSOR_NONE { };

}

namespace BAN::Formatter
{

	template<typename F>
	void print_argument(F putc, Kernel::ProcessorID processor_id, const ValueFormat& format)
	{
		print_argument(putc, processor_id.as_u32(), format);
	}

}
