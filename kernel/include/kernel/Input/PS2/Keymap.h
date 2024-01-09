#pragma once

#include <BAN/Array.h>
#include <BAN/Optional.h>

namespace Kernel::Input
{

	class PS2Keymap
	{
	public:
		void initialize(uint8_t scancode_set);

		BAN::Optional<uint8_t> get_keycode(uint8_t scancode, bool extended) const;

	private:
		void initialize_scancode_set1();
		void initialize_scancode_set2();
		void initialize_scancode_set3();

	private:
		BAN::Array<uint8_t, 0xFF> m_scancode_to_keycode_normal;
		BAN::Array<uint8_t, 0xFF> m_scancode_to_keycode_extended;
	};

}
