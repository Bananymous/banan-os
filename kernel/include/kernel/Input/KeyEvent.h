#pragma once

#include <stdint.h>

namespace Kernel::Input
{

	enum class Key
	{
		Invalid, None,
		A, B, C, D, E, F, G, H, I, J, K, L, M, N, O, P, Q, R, S, T, U, V, W, X, Y, Z,
		A_Ring, A_Umlaut, O_Umlaut,
		_0, _1, _2, _3, _4, _5, _6, _7, _8, _9,
		F1, F2, F3, F4, F5, F6, F7, F8, F9, F10, F11, F12,
		Insert, PrintScreen, Delete, Home, End, PageUp, PageDown, Enter, Space,
		ExclamationMark, DoubleQuote, Hashtag, Currency, Percent, Ampersand, Slash, Section, Half,
		OpenBracet, CloseBracet, OpenBrace, CloseBrace, OpenCurlyBrace, CloseCurlyBrace,
		Equals, QuestionMark, Plus, BackSlash, Acute, BackTick, TwoDots, Backspace, AtSign, Pound, Dollar, Euro,
		Escape, Tab, CapsLock, LeftShift, LeftCtrl, Super, Alt, AltGr, RightCtrl, RightShift,
		SingleQuote, Asterix, Caret, Tilde, ArrowUp, ArrowDown, ArrowLeft, ArrowRight,
		Comma, Semicolon, Period, Colon, Hyphen, Underscore, NumLock, ScrollLock, LessThan, GreaterThan, Pipe,
		Numpad0, Numpad1, Numpad2, Numpad3, Numpad4, Numpad5, Numpad6, Numpad7, Numpad8, Numpad9,
		NumpadPlus, NumpadMinus, NumpadMultiply, NumpadDivide, NumpadEnter, NumpadDecimal,
		VolumeMute, VolumeUp, VolumeDown, Calculator, MediaPlayPause, MediaStop, MediaPrevious, MediaNext,
		Count,
	};

	struct KeyEvent
	{
		enum class Modifier : uint8_t
		{
			Shift		= (1 << 0),
			Ctrl		= (1 << 1),
			Alt			= (1 << 2),
			AltGr		= (1 << 3),
			CapsLock	= (1 << 4),
			NumLock		= (1 << 5),
			ScrollLock	= (1 << 6),
			Released	= (1 << 7),
		};

		bool shift()		const { return modifier & (uint8_t)Modifier::Shift; }
		bool ctrl()			const { return modifier & (uint8_t)Modifier::Ctrl; }
		bool alt()			const { return modifier & (uint8_t)Modifier::Alt; }
		bool altgr()		const { return modifier & (uint8_t)Modifier::AltGr; }
		bool caps_lock()	const { return modifier & (uint8_t)Modifier::CapsLock; }
		bool num_lock()		const { return modifier & (uint8_t)Modifier::NumLock; }
		bool scroll_lock()	const { return modifier & (uint8_t)Modifier::ScrollLock; }
		bool released()		const { return modifier & (uint8_t)Modifier::Released; }
		bool pressed()		const { return !released(); }

		uint8_t modifier;
		Key key;
	};

	inline const char* key_event_to_utf8(KeyEvent event)
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
			"=", "?", "+", "\\", "´", "`", "¨", nullptr, "@", "£", "$", "€",
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
			"=", "?", "+", "\\", "´", "`", "¨", nullptr, "@", "£", "$", "€",
			nullptr, "\t", nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
			"'", "*", "^", "~", nullptr, nullptr, nullptr, nullptr,
			",", ";", ".", ":", "-", "_", nullptr, nullptr, "<", ">", "|",
			"0", "1", "2", "3", "4", "5", "6", "7", "8", "9",
			"+", "-", "*", "/", nullptr, ",",
			nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
		};
		static_assert((size_t)Key::Count == sizeof(utf8_upper) / sizeof(*utf8_lower));

		return (event.shift() ^ event.caps_lock()) ? utf8_upper[(uint8_t)event.key] : utf8_lower[(uint8_t)event.key];
	}

}