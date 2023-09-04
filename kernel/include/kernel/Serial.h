#pragma once

#include <BAN/Errors.h>

namespace Kernel
{

	class Serial
	{
	public:
		static void initialize();
		static bool has_devices();
		static void putchar_any(char);

		void putchar(char);

	private:
		static bool port_has_device(uint16_t);

		bool is_transmit_empty() const;

		bool is_valid() const { return m_port != 0; }

	private:
		uint16_t m_port { 0 };
	};

}