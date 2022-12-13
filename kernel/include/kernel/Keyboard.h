#pragma once

#include <stdint.h>

namespace Keyboard
{

	enum class Key : uint8_t
	{
		INVALID, None,
		_0, _1, _2, _3, _4, _5, _6, _7, _8, _9,
		A, B, C, D, E, F, G, H, I, J, K, L, M, N, O, P, Q, R, S, T, U, V, W, X, Y, Z,
		A_Dot, A_Dots, O_Dots,

		Comma, Colon, Period, Semicolon, Hyphen, Underscore, SingleQuote, Asterix, Caret, Tilde,
		ExclamationMark, QuestionMark, DoubleQuote, Hashtag, Percent, Ampersand, Slash, BackSlash, Plus, Equals,
		OpenParen, CloseParen, OpenBracket, CloseBracket, OpenBrace, CloseBrace,
		Dollar, Pound, Euro, Currency, Enter, Space, Tab, Backspace, LessThan, MoreThan, Tick, BackTick, Section, Half, At, Pipe,
		End, Home, Insert, Delete, Super, PageUp, PageDown, PrintScreen, Left, Right, Up, Down,

		LeftShift, RightShift, CapsLock, LeftCtrl, RightCtrl, LeftAlt, RightAlt, NumLock, ScrollLock, Escape,

		Numpad0, Numpad1, Numpad2, Numpad3, Numpad4, Numpad5, Numpad6, Numpad7, Numpad8, Numpad9,
		NumpadSep, NumpadPlus, NumpadMult, NumpadDiv, NumpadMinus, NumpadEnter,

		Mute, VolumeDown, VolumeUp, Calculator, PlayPause, Stop, PreviousTrack, NextTrack,

		F1, F2, F3, F4, F5, F6, F7, F8, F9, F10, F11, F12,

		Count
	};

	struct KeyEvent
	{
		Key		key;
		uint8_t	modifiers;
		bool	pressed;
	};

	bool initialize();
	void update_keyboard();

	void register_key_event_callback(void(*callback)(KeyEvent));

	char key_event_to_ascii(KeyEvent);

	void led_disco();

}