#include <kernel/Debug.h>
#include <kernel/Input/PS2/Keymap.h>
#include <LibInput/KeyEvent.h>

#include <string.h>

namespace Kernel::Input
{
	using LibInput::keycode_function;
	using LibInput::keycode_normal;
	using LibInput::keycode_numpad;

	void PS2Keymap::initialize(uint8_t scancode_set)
	{
		memset(m_scancode_to_keycode_normal.data(),   0xFF, m_scancode_to_keycode_normal.size());
		memset(m_scancode_to_keycode_extended.data(), 0xFF, m_scancode_to_keycode_extended.size());
		if (scancode_set == 1)
			return initialize_scancode_set1();
		if (scancode_set == 2)
			return initialize_scancode_set2();
		if (scancode_set == 3)
			return initialize_scancode_set3();
		ASSERT_NOT_REACHED();
	}

	BAN::Optional<uint8_t> PS2Keymap::get_keycode(uint8_t scancode, bool extended) const
	{
		uint8_t keycode = extended ? m_scancode_to_keycode_extended[scancode] : m_scancode_to_keycode_normal[scancode];
		if (keycode == 0xFF)
		{
			dprintln("unknown {2H} {}", scancode, extended ? 'E' : ' ');
			return {};
		}
		return keycode;
	}

	void PS2Keymap::initialize_scancode_set1()
	{
		m_scancode_to_keycode_normal[0x29]   = keycode_normal(0,  0);
		m_scancode_to_keycode_normal[0x02]   = keycode_normal(0,  1);
		m_scancode_to_keycode_normal[0x03]   = keycode_normal(0,  2);
		m_scancode_to_keycode_normal[0x04]   = keycode_normal(0,  3);
		m_scancode_to_keycode_normal[0x05]   = keycode_normal(0,  4);
		m_scancode_to_keycode_normal[0x06]   = keycode_normal(0,  5);
		m_scancode_to_keycode_normal[0x07]   = keycode_normal(0,  6);
		m_scancode_to_keycode_normal[0x08]   = keycode_normal(0,  7);
		m_scancode_to_keycode_normal[0x09]   = keycode_normal(0,  8);
		m_scancode_to_keycode_normal[0x0A]   = keycode_normal(0,  9);
		m_scancode_to_keycode_normal[0x0B]   = keycode_normal(0, 10);
		m_scancode_to_keycode_normal[0x0C]   = keycode_normal(0, 11);
		m_scancode_to_keycode_normal[0x0D]   = keycode_normal(0, 12);
		m_scancode_to_keycode_normal[0x0E]   = keycode_normal(0, 13);
		m_scancode_to_keycode_normal[0x0F]   = keycode_normal(1,  0);
		m_scancode_to_keycode_normal[0x10]   = keycode_normal(1,  1);
		m_scancode_to_keycode_normal[0x11]   = keycode_normal(1,  2);
		m_scancode_to_keycode_normal[0x12]   = keycode_normal(1,  3);
		m_scancode_to_keycode_normal[0x13]   = keycode_normal(1,  4);
		m_scancode_to_keycode_normal[0x14]   = keycode_normal(1,  5);
		m_scancode_to_keycode_normal[0x15]   = keycode_normal(1,  6);
		m_scancode_to_keycode_normal[0x16]   = keycode_normal(1,  7);
		m_scancode_to_keycode_normal[0x17]   = keycode_normal(1,  8);
		m_scancode_to_keycode_normal[0x18]   = keycode_normal(1,  9);
		m_scancode_to_keycode_normal[0x19]   = keycode_normal(1, 10);
		m_scancode_to_keycode_normal[0x1A]   = keycode_normal(1, 11);
		m_scancode_to_keycode_normal[0x1B]   = keycode_normal(1, 12);
		m_scancode_to_keycode_normal[0x3A]   = keycode_normal(2,  0);
		m_scancode_to_keycode_normal[0x1E]   = keycode_normal(2,  1);
		m_scancode_to_keycode_normal[0x1F]   = keycode_normal(2,  2);
		m_scancode_to_keycode_normal[0x20]   = keycode_normal(2,  3);
		m_scancode_to_keycode_normal[0x21]   = keycode_normal(2,  4);
		m_scancode_to_keycode_normal[0x22]   = keycode_normal(2,  5);
		m_scancode_to_keycode_normal[0x23]   = keycode_normal(2,  6);
		m_scancode_to_keycode_normal[0x24]   = keycode_normal(2,  7);
		m_scancode_to_keycode_normal[0x25]   = keycode_normal(2,  8);
		m_scancode_to_keycode_normal[0x26]   = keycode_normal(2,  9);
		m_scancode_to_keycode_normal[0x27]   = keycode_normal(2, 10);
		m_scancode_to_keycode_normal[0x28]   = keycode_normal(2, 11);
		m_scancode_to_keycode_normal[0x2B]   = keycode_normal(2, 12);
		m_scancode_to_keycode_normal[0x1C]   = keycode_normal(2, 13);
		m_scancode_to_keycode_normal[0x2A]   = keycode_normal(3,  0);
		m_scancode_to_keycode_normal[0x56]   = keycode_normal(3,  1);
		m_scancode_to_keycode_normal[0x2C]   = keycode_normal(3,  2);
		m_scancode_to_keycode_normal[0x2D]   = keycode_normal(3,  3);
		m_scancode_to_keycode_normal[0x2E]   = keycode_normal(3,  4);
		m_scancode_to_keycode_normal[0x2F]   = keycode_normal(3,  5);
		m_scancode_to_keycode_normal[0x30]   = keycode_normal(3,  6);
		m_scancode_to_keycode_normal[0x31]   = keycode_normal(3,  7);
		m_scancode_to_keycode_normal[0x32]   = keycode_normal(3,  8);
		m_scancode_to_keycode_normal[0x33]   = keycode_normal(3,  9);
		m_scancode_to_keycode_normal[0x34]   = keycode_normal(3, 10);
		m_scancode_to_keycode_normal[0x35]   = keycode_normal(3, 11);
		m_scancode_to_keycode_normal[0x36]   = keycode_normal(3, 12);
		m_scancode_to_keycode_normal[0x1D]   = keycode_normal(4,  0);
		m_scancode_to_keycode_extended[0x5B] = keycode_normal(4,  1);
		m_scancode_to_keycode_normal[0x38]   = keycode_normal(4,  2);
		m_scancode_to_keycode_normal[0x39]   = keycode_normal(4,  3);
		m_scancode_to_keycode_extended[0x38] = keycode_normal(4,  4);
		m_scancode_to_keycode_extended[0x1D] = keycode_normal(4,  5);

		m_scancode_to_keycode_normal[0x45]   = keycode_numpad(0, 0);
		m_scancode_to_keycode_extended[0x35] = keycode_numpad(0, 1);
		m_scancode_to_keycode_normal[0x37]   = keycode_numpad(0, 2);
		m_scancode_to_keycode_normal[0x4A]   = keycode_numpad(0, 3);
		m_scancode_to_keycode_normal[0x47]   = keycode_numpad(1, 0);
		m_scancode_to_keycode_normal[0x48]   = keycode_numpad(1, 1);
		m_scancode_to_keycode_normal[0x49]   = keycode_numpad(1, 2);
		m_scancode_to_keycode_normal[0x4E]   = keycode_numpad(1, 3);
		m_scancode_to_keycode_normal[0x4B]   = keycode_numpad(2, 0);
		m_scancode_to_keycode_normal[0x4C]   = keycode_numpad(2, 1);
		m_scancode_to_keycode_normal[0x4D]   = keycode_numpad(2, 2);
		m_scancode_to_keycode_normal[0x4F]   = keycode_numpad(3, 0);
		m_scancode_to_keycode_normal[0x50]   = keycode_numpad(3, 1);
		m_scancode_to_keycode_normal[0x51]   = keycode_numpad(3, 2);
		m_scancode_to_keycode_extended[0x1C] = keycode_numpad(3, 3);
		m_scancode_to_keycode_normal[0x52]   = keycode_numpad(4, 0);
		m_scancode_to_keycode_normal[0x53]   = keycode_numpad(4, 1);

		m_scancode_to_keycode_normal[0x01]   = keycode_function( 0);
		m_scancode_to_keycode_normal[0x3B]   = keycode_function( 1);
		m_scancode_to_keycode_normal[0x3C]   = keycode_function( 2);
		m_scancode_to_keycode_normal[0x3D]   = keycode_function( 3);
		m_scancode_to_keycode_normal[0x3E]   = keycode_function( 4);
		m_scancode_to_keycode_normal[0x3F]   = keycode_function( 5);
		m_scancode_to_keycode_normal[0x40]   = keycode_function( 6);
		m_scancode_to_keycode_normal[0x41]   = keycode_function( 7);
		m_scancode_to_keycode_normal[0x42]   = keycode_function( 8);
		m_scancode_to_keycode_normal[0x43]   = keycode_function( 9);
		m_scancode_to_keycode_normal[0x44]   = keycode_function(10);
		m_scancode_to_keycode_normal[0x57]   = keycode_function(11);
		m_scancode_to_keycode_normal[0x58]   = keycode_function(12);
		m_scancode_to_keycode_extended[0x52] = keycode_function(13);
		//m_scancode_to_keycode_normal[0x]   = keycode_function(14);
		m_scancode_to_keycode_extended[0x53] = keycode_function(15);
		m_scancode_to_keycode_extended[0x47] = keycode_function(16);
		m_scancode_to_keycode_extended[0x4F] = keycode_function(17);
		m_scancode_to_keycode_extended[0x49] = keycode_function(18);
		m_scancode_to_keycode_extended[0x51] = keycode_function(19);
		m_scancode_to_keycode_normal[0x46]   = keycode_function(20);

		// Arrow keys
		m_scancode_to_keycode_extended[0x48] = keycode_normal(5, 0);
		m_scancode_to_keycode_extended[0x4B] = keycode_normal(5, 1);
		m_scancode_to_keycode_extended[0x50] = keycode_normal(5, 2);
		m_scancode_to_keycode_extended[0x4D] = keycode_normal(5, 3);
	}

	void PS2Keymap::initialize_scancode_set2()
	{
		m_scancode_to_keycode_normal[0x0E]   = keycode_normal(0,  0);
		m_scancode_to_keycode_normal[0x16]   = keycode_normal(0,  1);
		m_scancode_to_keycode_normal[0x1E]   = keycode_normal(0,  2);
		m_scancode_to_keycode_normal[0x26]   = keycode_normal(0,  3);
		m_scancode_to_keycode_normal[0x25]   = keycode_normal(0,  4);
		m_scancode_to_keycode_normal[0x2E]   = keycode_normal(0,  5);
		m_scancode_to_keycode_normal[0x36]   = keycode_normal(0,  6);
		m_scancode_to_keycode_normal[0x3D]   = keycode_normal(0,  7);
		m_scancode_to_keycode_normal[0x3E]   = keycode_normal(0,  8);
		m_scancode_to_keycode_normal[0x46]   = keycode_normal(0,  9);
		m_scancode_to_keycode_normal[0x45]   = keycode_normal(0, 10);
		m_scancode_to_keycode_normal[0x4E]   = keycode_normal(0, 11);
		m_scancode_to_keycode_normal[0x55]   = keycode_normal(0, 12);
		m_scancode_to_keycode_normal[0x66]   = keycode_normal(0, 13);
		m_scancode_to_keycode_normal[0x0D]   = keycode_normal(1,  0);
		m_scancode_to_keycode_normal[0x15]   = keycode_normal(1,  1);
		m_scancode_to_keycode_normal[0x1D]   = keycode_normal(1,  2);
		m_scancode_to_keycode_normal[0x24]   = keycode_normal(1,  3);
		m_scancode_to_keycode_normal[0x2D]   = keycode_normal(1,  4);
		m_scancode_to_keycode_normal[0x2C]   = keycode_normal(1,  5);
		m_scancode_to_keycode_normal[0x35]   = keycode_normal(1,  6);
		m_scancode_to_keycode_normal[0x3C]   = keycode_normal(1,  7);
		m_scancode_to_keycode_normal[0x43]   = keycode_normal(1,  8);
		m_scancode_to_keycode_normal[0x44]   = keycode_normal(1,  9);
		m_scancode_to_keycode_normal[0x4D]   = keycode_normal(1, 10);
		m_scancode_to_keycode_normal[0x54]   = keycode_normal(1, 11);
		m_scancode_to_keycode_normal[0x5B]   = keycode_normal(1, 12);
		m_scancode_to_keycode_normal[0x58]   = keycode_normal(2,  0);
		m_scancode_to_keycode_normal[0x1C]   = keycode_normal(2,  1);
		m_scancode_to_keycode_normal[0x1B]   = keycode_normal(2,  2);
		m_scancode_to_keycode_normal[0x23]   = keycode_normal(2,  3);
		m_scancode_to_keycode_normal[0x2B]   = keycode_normal(2,  4);
		m_scancode_to_keycode_normal[0x34]   = keycode_normal(2,  5);
		m_scancode_to_keycode_normal[0x33]   = keycode_normal(2,  6);
		m_scancode_to_keycode_normal[0x3B]   = keycode_normal(2,  7);
		m_scancode_to_keycode_normal[0x42]   = keycode_normal(2,  8);
		m_scancode_to_keycode_normal[0x4B]   = keycode_normal(2,  9);
		m_scancode_to_keycode_normal[0x4C]   = keycode_normal(2, 10);
		m_scancode_to_keycode_normal[0x52]   = keycode_normal(2, 11);
		m_scancode_to_keycode_normal[0x5D]   = keycode_normal(2, 12);
		m_scancode_to_keycode_normal[0x5A]   = keycode_normal(2, 13);
		m_scancode_to_keycode_normal[0x12]   = keycode_normal(3,  0);
		m_scancode_to_keycode_normal[0x61]   = keycode_normal(3,  1);
		m_scancode_to_keycode_normal[0x1A]   = keycode_normal(3,  2);
		m_scancode_to_keycode_normal[0x22]   = keycode_normal(3,  3);
		m_scancode_to_keycode_normal[0x21]   = keycode_normal(3,  4);
		m_scancode_to_keycode_normal[0x2A]   = keycode_normal(3,  5);
		m_scancode_to_keycode_normal[0x32]   = keycode_normal(3,  6);
		m_scancode_to_keycode_normal[0x31]   = keycode_normal(3,  7);
		m_scancode_to_keycode_normal[0x3A]   = keycode_normal(3,  8);
		m_scancode_to_keycode_normal[0x41]   = keycode_normal(3,  9);
		m_scancode_to_keycode_normal[0x49]   = keycode_normal(3, 10);
		m_scancode_to_keycode_normal[0x4A]   = keycode_normal(3, 11);
		m_scancode_to_keycode_normal[0x59]   = keycode_normal(3, 12);
		m_scancode_to_keycode_normal[0x14]   = keycode_normal(4,  0);
		m_scancode_to_keycode_extended[0x1F] = keycode_normal(4,  1);
		m_scancode_to_keycode_normal[0x11]   = keycode_normal(4,  2);
		m_scancode_to_keycode_normal[0x29]   = keycode_normal(4,  3);
		m_scancode_to_keycode_extended[0x11] = keycode_normal(4,  4);
		m_scancode_to_keycode_extended[0x14] = keycode_normal(4,  5);

		m_scancode_to_keycode_normal[0x77]   = keycode_numpad(0, 0);
		m_scancode_to_keycode_extended[0x4A] = keycode_numpad(0, 1);
		m_scancode_to_keycode_normal[0x7C]   = keycode_numpad(0, 2);
		m_scancode_to_keycode_normal[0x7B]   = keycode_numpad(0, 3);
		m_scancode_to_keycode_normal[0x6C]   = keycode_numpad(1, 0);
		m_scancode_to_keycode_normal[0x75]   = keycode_numpad(1, 1);
		m_scancode_to_keycode_normal[0x7D]   = keycode_numpad(1, 2);
		m_scancode_to_keycode_normal[0x79]   = keycode_numpad(1, 3);
		m_scancode_to_keycode_normal[0x6B]   = keycode_numpad(2, 0);
		m_scancode_to_keycode_normal[0x73]   = keycode_numpad(2, 1);
		m_scancode_to_keycode_normal[0x74]   = keycode_numpad(2, 2);
		m_scancode_to_keycode_normal[0x69]   = keycode_numpad(3, 0);
		m_scancode_to_keycode_normal[0x72]   = keycode_numpad(3, 1);
		m_scancode_to_keycode_normal[0x7A]   = keycode_numpad(3, 2);
		m_scancode_to_keycode_extended[0x5A] = keycode_numpad(3, 3);
		m_scancode_to_keycode_normal[0x70]   = keycode_numpad(4, 0);
		m_scancode_to_keycode_normal[0x71]   = keycode_numpad(4, 1);

		m_scancode_to_keycode_normal[0x76]   = keycode_function( 0);
		m_scancode_to_keycode_normal[0x05]   = keycode_function( 1);
		m_scancode_to_keycode_normal[0x06]   = keycode_function( 2);
		m_scancode_to_keycode_normal[0x04]   = keycode_function( 3);
		m_scancode_to_keycode_normal[0x0C]   = keycode_function( 4);
		m_scancode_to_keycode_normal[0x03]   = keycode_function( 5);
		m_scancode_to_keycode_normal[0x0B]   = keycode_function( 6);
		m_scancode_to_keycode_normal[0x83]   = keycode_function( 7);
		m_scancode_to_keycode_normal[0x0A]   = keycode_function( 8);
		m_scancode_to_keycode_normal[0x01]   = keycode_function( 9);
		m_scancode_to_keycode_normal[0x09]   = keycode_function(10);
		m_scancode_to_keycode_normal[0x78]   = keycode_function(11);
		m_scancode_to_keycode_normal[0x07]   = keycode_function(12);
		m_scancode_to_keycode_extended[0x70] = keycode_function(13);
		//m_scancode_to_keycode_normal[0x]   = keycode_function(14);
		m_scancode_to_keycode_extended[0x71] = keycode_function(15);
		m_scancode_to_keycode_extended[0x6C] = keycode_function(16);
		m_scancode_to_keycode_extended[0x69] = keycode_function(17);
		m_scancode_to_keycode_extended[0x7D] = keycode_function(18);
		m_scancode_to_keycode_extended[0x7A] = keycode_function(19);
		m_scancode_to_keycode_normal[0x7E]   = keycode_function(20);

		// Arrow keys
		m_scancode_to_keycode_extended[0x75] = keycode_normal(5, 0);
		m_scancode_to_keycode_extended[0x6B] = keycode_normal(5, 1);
		m_scancode_to_keycode_extended[0x72] = keycode_normal(5, 2);
		m_scancode_to_keycode_extended[0x74] = keycode_normal(5, 3);
	}

	void PS2Keymap::initialize_scancode_set3()
	{
		m_scancode_to_keycode_normal[0x0E]   = keycode_normal(0,  0);
		m_scancode_to_keycode_normal[0x16]   = keycode_normal(0,  1);
		m_scancode_to_keycode_normal[0x1E]   = keycode_normal(0,  2);
		m_scancode_to_keycode_normal[0x26]   = keycode_normal(0,  3);
		m_scancode_to_keycode_normal[0x25]   = keycode_normal(0,  4);
		m_scancode_to_keycode_normal[0x2E]   = keycode_normal(0,  5);
		m_scancode_to_keycode_normal[0x36]   = keycode_normal(0,  6);
		m_scancode_to_keycode_normal[0x3D]   = keycode_normal(0,  7);
		m_scancode_to_keycode_normal[0x3E]   = keycode_normal(0,  8);
		m_scancode_to_keycode_normal[0x46]   = keycode_normal(0,  9);
		m_scancode_to_keycode_normal[0x45]   = keycode_normal(0, 10);
		m_scancode_to_keycode_normal[0x4E]   = keycode_normal(0, 11);
		m_scancode_to_keycode_normal[0x55]   = keycode_normal(0, 12);
		m_scancode_to_keycode_normal[0x66]   = keycode_normal(0, 13);
		m_scancode_to_keycode_normal[0x0D]   = keycode_normal(1,  0);
		m_scancode_to_keycode_normal[0x15]   = keycode_normal(1,  1);
		m_scancode_to_keycode_normal[0x1D]   = keycode_normal(1,  2);
		m_scancode_to_keycode_normal[0x24]   = keycode_normal(1,  3);
		m_scancode_to_keycode_normal[0x2D]   = keycode_normal(1,  4);
		m_scancode_to_keycode_normal[0x2C]   = keycode_normal(1,  5);
		m_scancode_to_keycode_normal[0x35]   = keycode_normal(1,  6);
		m_scancode_to_keycode_normal[0x3C]   = keycode_normal(1,  7);
		m_scancode_to_keycode_normal[0x43]   = keycode_normal(1,  8);
		m_scancode_to_keycode_normal[0x44]   = keycode_normal(1,  9);
		m_scancode_to_keycode_normal[0x4D]   = keycode_normal(1, 10);
		m_scancode_to_keycode_normal[0x54]   = keycode_normal(1, 11);
		m_scancode_to_keycode_normal[0x5B]   = keycode_normal(1, 12);
		m_scancode_to_keycode_normal[0x14]   = keycode_normal(2,  0);
		m_scancode_to_keycode_normal[0x1C]   = keycode_normal(2,  1);
		m_scancode_to_keycode_normal[0x1B]   = keycode_normal(2,  2);
		m_scancode_to_keycode_normal[0x23]   = keycode_normal(2,  3);
		m_scancode_to_keycode_normal[0x2B]   = keycode_normal(2,  4);
		m_scancode_to_keycode_normal[0x34]   = keycode_normal(2,  5);
		m_scancode_to_keycode_normal[0x33]   = keycode_normal(2,  6);
		m_scancode_to_keycode_normal[0x3B]   = keycode_normal(2,  7);
		m_scancode_to_keycode_normal[0x42]   = keycode_normal(2,  8);
		m_scancode_to_keycode_normal[0x4B]   = keycode_normal(2,  9);
		m_scancode_to_keycode_normal[0x4C]   = keycode_normal(2, 10);
		m_scancode_to_keycode_normal[0x52]   = keycode_normal(2, 11);
		m_scancode_to_keycode_normal[0x5C]   = keycode_normal(2, 12);
		m_scancode_to_keycode_normal[0x5A]   = keycode_normal(2, 13);
		m_scancode_to_keycode_normal[0x12]   = keycode_normal(3,  0);
		m_scancode_to_keycode_normal[0x13]   = keycode_normal(3,  1);
		m_scancode_to_keycode_normal[0x1A]   = keycode_normal(3,  2);
		m_scancode_to_keycode_normal[0x22]   = keycode_normal(3,  3);
		m_scancode_to_keycode_normal[0x21]   = keycode_normal(3,  4);
		m_scancode_to_keycode_normal[0x2A]   = keycode_normal(3,  5);
		m_scancode_to_keycode_normal[0x32]   = keycode_normal(3,  6);
		m_scancode_to_keycode_normal[0x31]   = keycode_normal(3,  7);
		m_scancode_to_keycode_normal[0x3A]   = keycode_normal(3,  8);
		m_scancode_to_keycode_normal[0x41]   = keycode_normal(3,  9);
		m_scancode_to_keycode_normal[0x49]   = keycode_normal(3, 10);
		m_scancode_to_keycode_normal[0x4A]   = keycode_normal(3, 11);
		m_scancode_to_keycode_normal[0x59]   = keycode_normal(3, 12);
		m_scancode_to_keycode_normal[0x11]   = keycode_normal(4,  0);
		m_scancode_to_keycode_normal[0x8B]   = keycode_normal(4,  1);
		m_scancode_to_keycode_normal[0x19]   = keycode_normal(4,  2);
		m_scancode_to_keycode_normal[0x29]   = keycode_normal(4,  3);
		m_scancode_to_keycode_normal[0x39]   = keycode_normal(4,  4);
		m_scancode_to_keycode_normal[0x58]   = keycode_normal(4,  5);

		m_scancode_to_keycode_normal[0x76] = keycode_numpad(0, 0);
		//m_scancode_to_keycode_normal[0x] = keycode_numpad(0, 1);
		m_scancode_to_keycode_normal[0x7E] = keycode_numpad(0, 2);
		//m_scancode_to_keycode_normal[0x] = keycode_numpad(0, 3);
		m_scancode_to_keycode_normal[0x6C] = keycode_numpad(1, 1);
		m_scancode_to_keycode_normal[0x75] = keycode_numpad(1, 2);
		m_scancode_to_keycode_normal[0x7D] = keycode_numpad(1, 3);
		m_scancode_to_keycode_normal[0x7C] = keycode_numpad(1, 4);
		m_scancode_to_keycode_normal[0x6B] = keycode_numpad(2, 1);
		m_scancode_to_keycode_normal[0x73] = keycode_numpad(2, 2);
		m_scancode_to_keycode_normal[0x74] = keycode_numpad(2, 3);
		m_scancode_to_keycode_normal[0x69] = keycode_numpad(3, 1);
		m_scancode_to_keycode_normal[0x72] = keycode_numpad(3, 2);
		m_scancode_to_keycode_normal[0x7A] = keycode_numpad(3, 3);
		m_scancode_to_keycode_normal[0x79] = keycode_numpad(3, 4);
		m_scancode_to_keycode_normal[0x70] = keycode_numpad(4, 1);
		m_scancode_to_keycode_normal[0x71] = keycode_numpad(4, 2);

		m_scancode_to_keycode_normal[0x08] = keycode_function(0);
		m_scancode_to_keycode_normal[0x07] = keycode_function(1);
		m_scancode_to_keycode_normal[0x0F] = keycode_function(2);
		m_scancode_to_keycode_normal[0x17] = keycode_function(3);
		m_scancode_to_keycode_normal[0x1F] = keycode_function(4);
		m_scancode_to_keycode_normal[0x27] = keycode_function(5);
		m_scancode_to_keycode_normal[0x2F] = keycode_function(6);
		m_scancode_to_keycode_normal[0x37] = keycode_function(7);
		m_scancode_to_keycode_normal[0x3F] = keycode_function(8);
		m_scancode_to_keycode_normal[0x47] = keycode_function(9);
		m_scancode_to_keycode_normal[0x4F] = keycode_function(10);
		m_scancode_to_keycode_normal[0x56] = keycode_function(11);
		m_scancode_to_keycode_normal[0x5E] = keycode_function(12);
		m_scancode_to_keycode_normal[0x67] = keycode_function(13);
		m_scancode_to_keycode_normal[0x57] = keycode_function(14);
		m_scancode_to_keycode_normal[0x64] = keycode_function(15);
		m_scancode_to_keycode_normal[0x6E] = keycode_function(16);
		m_scancode_to_keycode_normal[0x65] = keycode_function(17);
		m_scancode_to_keycode_normal[0x6F] = keycode_function(18);
		m_scancode_to_keycode_normal[0x6D] = keycode_function(19);
		m_scancode_to_keycode_normal[0x5F] = keycode_function(20);

		// Arrow keys
		m_scancode_to_keycode_normal[0x63] = keycode_normal(5, 0);
		m_scancode_to_keycode_normal[0x61] = keycode_normal(5, 1);
		m_scancode_to_keycode_normal[0x60] = keycode_normal(5, 2);
		m_scancode_to_keycode_normal[0x6A] = keycode_normal(5, 3);
	}

}
