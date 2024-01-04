#pragma once

#include <BAN/Vector.h>
#include <kernel/Input/KeyEvent.h>

namespace Kernel::Input
{

	class PS2Keymap
	{
	public:
		PS2Keymap();

		Key key_for_scancode_and_modifiers(uint32_t, uint8_t);

	private:
		BAN::Vector<Key> m_normal_keymap;
		BAN::Vector<Key> m_shift_keymap;
		BAN::Vector<Key> m_altgr_keymap;
		BAN::Vector<Key> m_extended_keymap;
	};

}
