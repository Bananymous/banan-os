#include <BAN/Array.h>
#include <kernel/Input/KeyEvent.h>

namespace Kernel::Input
{

	BAN::Array<Key, 0xFF> g_keycode_to_key_normal;
	BAN::Array<Key, 0xFF> g_keycode_to_key_shift;
	BAN::Array<Key, 0xFF> g_keycode_to_key_altgr;

	Key key_event_to_key(KeyEvent event)
	{
		if (event.shift())
			return g_keycode_to_key_shift[event.keycode];
		if (event.ralt())
			return g_keycode_to_key_altgr[event.keycode];
		return g_keycode_to_key_normal[event.keycode];
	}

	const char* key_to_utf8(Key key, uint16_t modifier)
	{
		static constexpr const char* utf8_lower[] = {
			nullptr, nullptr,
			"a", "b", "c", "d", "e", "f", "g", "h", "i", "j", "k", "l", "m", "n", "o", "p", "q", "r", "s", "t", "u", "v", "w", "x", "y", "z",
			"å", "ä", "ö",
			"0", "1", "2", "3", "4", "5", "6", "7", "8", "9",
			nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
			nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, " ",
			"!", "\"", "#", "¤", "%", "&", "/", "§", "½",
			"(", ")", "[", "]", "{", "}",
			"=", "?", "+", "\\", "´", "`", "¨", "¸", nullptr, "@", "£", "$", "€",
			nullptr, "\t", nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
			"'", "*", "^", "~", nullptr, nullptr, nullptr, nullptr,
			",", ";", ".", ":", "-", "_", nullptr, nullptr, "<", ">", "|",
			"0", "1", "2", "3", "4", "5", "6", "7", "8", "9",
			"+", "-", "*", "/", nullptr, ",",
			nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
		};
		static_assert((size_t)Key::Count == sizeof(utf8_lower) / sizeof(*utf8_lower));

		static constexpr const char* utf8_upper[] = {
			nullptr, nullptr,
			"A", "B", "C", "D", "E", "F", "G", "H", "I", "J", "K", "L", "M", "N", "O", "P", "Q", "R", "S", "T", "U", "V", "W", "X", "Y", "Z",
			"Å", "Ä", "Ö",
			"0", "1", "2", "3", "4", "5", "6", "7", "8", "9",
			nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
			nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, " ",
			"!", "\"", "#", "¤", "%", "&", "/", "§", "½",
			"(", ")", "[", "]", "{", "}",
			"=", "?", "+", "\\", "´", "`", "¨", "¸", nullptr, "@", "£", "$", "€",
			nullptr, "\t", nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
			"'", "*", "^", "~", nullptr, nullptr, nullptr, nullptr,
			",", ";", ".", ":", "-", "_", nullptr, nullptr, "<", ">", "|",
			"0", "1", "2", "3", "4", "5", "6", "7", "8", "9",
			"+", "-", "*", "/", nullptr, ",",
			nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
		};
		static_assert((size_t)Key::Count == sizeof(utf8_upper) / sizeof(*utf8_lower));

		KeyEvent event { .modifier = modifier, .keycode = 0x00 };
		return (event.shift() ^ event.caps_lock()) ? utf8_upper[static_cast<uint8_t>(key)] : utf8_lower[static_cast<uint8_t>(key)];
	}

}
