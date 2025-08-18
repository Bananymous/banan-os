#pragma once

#include <stdint.h>

namespace LibInput
{

	/*
		Key Code:
			bits 4:0 column (from left)
			bits 7:5 row    (from top)
	*/

	#define BANAN_CONSTEVAL_STATIC_ASSERT(cond) do { int dummy = 1 / (cond); (void)dummy; } while (false)

	consteval uint8_t keycode_function(uint8_t index)
	{
		BANAN_CONSTEVAL_STATIC_ASSERT(index <= 0b11111);
		return index;
	}

	consteval uint8_t keycode_normal(uint8_t row, uint8_t col)
	{
		BANAN_CONSTEVAL_STATIC_ASSERT(row <= 0b111 - 1);
		BANAN_CONSTEVAL_STATIC_ASSERT(col <= 0b11111 - 8);
		return ((row + 1) << 5) | col;
	}

	consteval uint8_t keycode_numpad(uint8_t row, uint8_t col)
	{
		BANAN_CONSTEVAL_STATIC_ASSERT(row <= 0b111 - 1);
		BANAN_CONSTEVAL_STATIC_ASSERT(col < 8);
		return ((row + 1) << 5) | (col + 0b11111 - 8 + 1);
	}

	enum class Key
	{
		Invalid, None,
		A, B, C, D, E, F, G, H, I, J, K, L, M, N, O, P, Q, R, S, T, U, V, W, X, Y, Z,
		A_Ring, A_Umlaut, O_Umlaut,
		_0, _1, _2, _3, _4, _5, _6, _7, _8, _9,
		F1, F2, F3, F4, F5, F6, F7, F8, F9, F10, F11, F12,
		Insert, PrintScreen, Delete, Home, End, PageUp, PageDown, Enter, Space,
		ExclamationMark, DoubleQuote, Hashtag, Currency, Percent, Ampersand, Slash, Section, Half,
		OpenParenthesis, CloseParenthesis, OpenSquareBracket, CloseSquareBracket, OpenCurlyBracket, CloseCurlyBracket,
		Equals, QuestionMark, Plus, BackSlash, Acute, BackTick, TwoDots, Cedilla, Backspace, AtSign, Pound, Dollar, Euro,
		Escape, Tab, CapsLock, LeftShift, LeftCtrl, Super, LeftAlt, RightAlt, AltGr = RightAlt, RightCtrl, RightShift,
		SingleQuote, Asterix, Caret, Tilde, ArrowUp, ArrowDown, ArrowLeft, ArrowRight,
		Comma, Semicolon, Period, Colon, Hyphen, Underscore, NumLock, ScrollLock, LessThan, GreaterThan, Pipe, Negation, BrokenBar,
		Numpad0, Numpad1, Numpad2, Numpad3, Numpad4, Numpad5, Numpad6, Numpad7, Numpad8, Numpad9,
		NumpadPlus, NumpadMinus, NumpadMultiply, NumpadDivide, NumpadEnter, NumpadDecimal,
		VolumeMute, VolumeUp, VolumeDown, Calculator, MediaPlayPause, MediaStop, MediaPrevious, MediaNext,
		Count,
	};

	// KeyEvent with general keycode
	struct RawKeyEvent
	{
		uint16_t modifier;
		uint8_t keycode;
	};

	// KeyEvent with key parsed from keycode
	struct KeyEvent
	{
		enum Modifier : uint16_t
		{
			LShift		= (1 << 0),
			RShift		= (1 << 1),
			LCtrl		= (1 << 2),
			RCtrl		= (1 << 3),
			LAlt		= (1 << 4),
			RAlt		= (1 << 5),
			CapsLock	= (1 << 6),
			NumLock		= (1 << 7),
			ScrollLock	= (1 << 8),
			Pressed		= (1 << 9),
		};

		bool lshift()		const { return modifier & Modifier::LShift; }
		bool rshift()		const { return modifier & Modifier::RShift; }
		bool shift()		const { return lshift() || rshift(); }

		bool lctrl()		const { return modifier & Modifier::LCtrl; }
		bool rctrl()		const { return modifier & Modifier::RCtrl; }
		bool ctrl()			const { return lctrl() || rctrl(); }

		bool lalt()			const { return modifier & Modifier::LAlt; }
		bool ralt()			const { return modifier & Modifier::RAlt; }
		bool alt()			const { return lalt() || ralt(); }

		bool caps_lock()	const { return modifier & Modifier::CapsLock; }
		bool num_lock()		const { return modifier & Modifier::NumLock; }
		bool scroll_lock()	const { return modifier & Modifier::ScrollLock; }

		bool pressed()		const { return modifier & Modifier::Pressed; }
		bool released()		const { return !pressed(); }

		uint16_t modifier;
		uint8_t scancode;
		Key key;
	};

	const char* key_to_utf8(Key key, uint16_t modifier);
	const char* key_to_utf8_ansi(Key key, uint16_t modifier);

}
