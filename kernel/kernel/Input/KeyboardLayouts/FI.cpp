#include <BAN/Array.h>
#include <kernel/Input/KeyEvent.h>

namespace Kernel::Input
{

	extern BAN::Array<Key, 0xFF> g_keycode_to_key_normal;
	extern BAN::Array<Key, 0xFF> g_keycode_to_key_shift;
	extern BAN::Array<Key, 0xFF> g_keycode_to_key_altgr;

}

namespace Kernel::Input::KeyboardLayout
{
	
	static void initialize_fi_normal();
	static void initialize_fi_shift();
	static void initialize_fi_altgr();

	void initialize_fi()
	{
		for (auto& key : g_keycode_to_key_normal)
			key = Key::None;
		for (auto& key : g_keycode_to_key_shift)
			key = Key::None;
		for (auto& key : g_keycode_to_key_altgr)
			key = Key::None;
		initialize_fi_normal();
		initialize_fi_shift();
		initialize_fi_altgr();
	}

	static void initialize_fi_normal()
	{
		g_keycode_to_key_normal[keycode_normal(0,  0)] = Key::Section;
		g_keycode_to_key_normal[keycode_normal(0,  1)] = Key::_1;
		g_keycode_to_key_normal[keycode_normal(0,  2)] = Key::_2;
		g_keycode_to_key_normal[keycode_normal(0,  3)] = Key::_3;
		g_keycode_to_key_normal[keycode_normal(0,  4)] = Key::_4;
		g_keycode_to_key_normal[keycode_normal(0,  5)] = Key::_5;
		g_keycode_to_key_normal[keycode_normal(0,  6)] = Key::_6;
		g_keycode_to_key_normal[keycode_normal(0,  7)] = Key::_7;
		g_keycode_to_key_normal[keycode_normal(0,  8)] = Key::_8;
		g_keycode_to_key_normal[keycode_normal(0,  9)] = Key::_9;
		g_keycode_to_key_normal[keycode_normal(0, 10)] = Key::_0;
		g_keycode_to_key_normal[keycode_normal(0, 11)] = Key::Plus;
		g_keycode_to_key_normal[keycode_normal(0, 12)] = Key::Acute;
		g_keycode_to_key_normal[keycode_normal(0, 13)] = Key::Backspace;
		g_keycode_to_key_normal[keycode_normal(1,  0)] = Key::Tab;
		g_keycode_to_key_normal[keycode_normal(1,  1)] = Key::Q;
		g_keycode_to_key_normal[keycode_normal(1,  2)] = Key::W;
		g_keycode_to_key_normal[keycode_normal(1,  3)] = Key::E;
		g_keycode_to_key_normal[keycode_normal(1,  4)] = Key::R;
		g_keycode_to_key_normal[keycode_normal(1,  5)] = Key::T;
		g_keycode_to_key_normal[keycode_normal(1,  6)] = Key::Y;
		g_keycode_to_key_normal[keycode_normal(1,  7)] = Key::U;
		g_keycode_to_key_normal[keycode_normal(1,  8)] = Key::I;
		g_keycode_to_key_normal[keycode_normal(1,  9)] = Key::O;
		g_keycode_to_key_normal[keycode_normal(1, 10)] = Key::P;
		g_keycode_to_key_normal[keycode_normal(1, 11)] = Key::A_Ring;
		g_keycode_to_key_normal[keycode_normal(1, 12)] = Key::TwoDots;
		g_keycode_to_key_normal[keycode_normal(2,  0)] = Key::CapsLock;
		g_keycode_to_key_normal[keycode_normal(2,  1)] = Key::A;
		g_keycode_to_key_normal[keycode_normal(2,  2)] = Key::S;
		g_keycode_to_key_normal[keycode_normal(2,  3)] = Key::D;
		g_keycode_to_key_normal[keycode_normal(2,  4)] = Key::F;
		g_keycode_to_key_normal[keycode_normal(2,  5)] = Key::G;
		g_keycode_to_key_normal[keycode_normal(2,  6)] = Key::H;
		g_keycode_to_key_normal[keycode_normal(2,  7)] = Key::J;
		g_keycode_to_key_normal[keycode_normal(2,  8)] = Key::K;
		g_keycode_to_key_normal[keycode_normal(2,  9)] = Key::L;
		g_keycode_to_key_normal[keycode_normal(2, 10)] = Key::O_Umlaut;
		g_keycode_to_key_normal[keycode_normal(2, 11)] = Key::A_Umlaut;
		g_keycode_to_key_normal[keycode_normal(2, 12)] = Key::SingleQuote;
		g_keycode_to_key_normal[keycode_normal(2, 13)] = Key::Enter;
		g_keycode_to_key_normal[keycode_normal(3,  0)] = Key::LeftShift;
		g_keycode_to_key_normal[keycode_normal(3,  1)] = Key::LessThan;
		g_keycode_to_key_normal[keycode_normal(3,  2)] = Key::Z;
		g_keycode_to_key_normal[keycode_normal(3,  3)] = Key::X;
		g_keycode_to_key_normal[keycode_normal(3,  4)] = Key::C;
		g_keycode_to_key_normal[keycode_normal(3,  5)] = Key::V;
		g_keycode_to_key_normal[keycode_normal(3,  6)] = Key::B;
		g_keycode_to_key_normal[keycode_normal(3,  7)] = Key::N;
		g_keycode_to_key_normal[keycode_normal(3,  8)] = Key::M;
		g_keycode_to_key_normal[keycode_normal(3,  9)] = Key::Comma;
		g_keycode_to_key_normal[keycode_normal(3, 10)] = Key::Period;
		g_keycode_to_key_normal[keycode_normal(3, 11)] = Key::Hyphen;
		g_keycode_to_key_normal[keycode_normal(3, 12)] = Key::RightShift;
		g_keycode_to_key_normal[keycode_normal(4,  0)] = Key::LeftCtrl;
		g_keycode_to_key_normal[keycode_normal(4,  1)] = Key::Super;
		g_keycode_to_key_normal[keycode_normal(4,  2)] = Key::LeftAlt;
		g_keycode_to_key_normal[keycode_normal(4,  3)] = Key::Space;
		g_keycode_to_key_normal[keycode_normal(4,  4)] = Key::RightAlt;
		g_keycode_to_key_normal[keycode_normal(4,  5)] = Key::RightCtrl;

		g_keycode_to_key_normal[keycode_numpad(0, 0)] = Key::NumLock;
		g_keycode_to_key_normal[keycode_numpad(0, 1)] = Key::NumpadDivide;
		g_keycode_to_key_normal[keycode_numpad(0, 2)] = Key::NumpadMultiply;
		g_keycode_to_key_normal[keycode_numpad(0, 3)] = Key::NumpadMinus;
		g_keycode_to_key_normal[keycode_numpad(1, 0)] = Key::Numpad7;
		g_keycode_to_key_normal[keycode_numpad(1, 1)] = Key::Numpad8;
		g_keycode_to_key_normal[keycode_numpad(1, 2)] = Key::Numpad9;
		g_keycode_to_key_normal[keycode_numpad(1, 3)] = Key::NumpadPlus;
		g_keycode_to_key_normal[keycode_numpad(2, 0)] = Key::Numpad4;
		g_keycode_to_key_normal[keycode_numpad(2, 1)] = Key::Numpad5;
		g_keycode_to_key_normal[keycode_numpad(2, 2)] = Key::Numpad6;
		g_keycode_to_key_normal[keycode_numpad(3, 0)] = Key::Numpad1;
		g_keycode_to_key_normal[keycode_numpad(3, 1)] = Key::Numpad2;
		g_keycode_to_key_normal[keycode_numpad(3, 2)] = Key::Numpad3;
		g_keycode_to_key_normal[keycode_numpad(3, 3)] = Key::NumpadEnter;
		g_keycode_to_key_normal[keycode_numpad(4, 0)] = Key::Numpad0;
		g_keycode_to_key_normal[keycode_numpad(4, 1)] = Key::NumpadDecimal;

		g_keycode_to_key_normal[keycode_function( 0)] = Key::Escape;
		g_keycode_to_key_normal[keycode_function( 1)] = Key::F1;
		g_keycode_to_key_normal[keycode_function( 2)] = Key::F2;
		g_keycode_to_key_normal[keycode_function( 3)] = Key::F3;
		g_keycode_to_key_normal[keycode_function( 4)] = Key::F4;
		g_keycode_to_key_normal[keycode_function( 5)] = Key::F5;
		g_keycode_to_key_normal[keycode_function( 6)] = Key::F6;
		g_keycode_to_key_normal[keycode_function( 7)] = Key::F7;
		g_keycode_to_key_normal[keycode_function( 8)] = Key::F8;
		g_keycode_to_key_normal[keycode_function( 9)] = Key::F9;
		g_keycode_to_key_normal[keycode_function(10)] = Key::F10;
		g_keycode_to_key_normal[keycode_function(11)] = Key::F11;
		g_keycode_to_key_normal[keycode_function(12)] = Key::F12;
		g_keycode_to_key_normal[keycode_function(13)] = Key::Insert;
		g_keycode_to_key_normal[keycode_function(14)] = Key::PrintScreen;
		g_keycode_to_key_normal[keycode_function(15)] = Key::Delete;
		g_keycode_to_key_normal[keycode_function(16)] = Key::Home;
		g_keycode_to_key_normal[keycode_function(17)] = Key::End;
		g_keycode_to_key_normal[keycode_function(18)] = Key::PageUp;
		g_keycode_to_key_normal[keycode_function(19)] = Key::PageDown;
		g_keycode_to_key_normal[keycode_function(20)] = Key::ScrollLock;

		// Arrow keys
		g_keycode_to_key_normal[keycode_normal(5, 0)] = Key::ArrowUp;
		g_keycode_to_key_normal[keycode_normal(5, 1)] = Key::ArrowLeft;
		g_keycode_to_key_normal[keycode_normal(5, 2)] = Key::ArrowDown;
		g_keycode_to_key_normal[keycode_normal(5, 3)] = Key::ArrowRight;
	}

	static void initialize_fi_shift()
	{
		g_keycode_to_key_shift[keycode_normal(0,  0)] = Key::Half;
		g_keycode_to_key_shift[keycode_normal(0,  1)] = Key::ExclamationMark;
		g_keycode_to_key_shift[keycode_normal(0,  2)] = Key::DoubleQuote;
		g_keycode_to_key_shift[keycode_normal(0,  3)] = Key::Hashtag;
		g_keycode_to_key_shift[keycode_normal(0,  4)] = Key::Currency;
		g_keycode_to_key_shift[keycode_normal(0,  5)] = Key::Percent;
		g_keycode_to_key_shift[keycode_normal(0,  6)] = Key::Ampersand;
		g_keycode_to_key_shift[keycode_normal(0,  7)] = Key::Slash;
		g_keycode_to_key_shift[keycode_normal(0,  8)] = Key::OpenParenthesis;
		g_keycode_to_key_shift[keycode_normal(0,  9)] = Key::CloseParenthesis;
		g_keycode_to_key_shift[keycode_normal(0, 10)] = Key::Equals;
		g_keycode_to_key_shift[keycode_normal(0, 11)] = Key::QuestionMark;
		g_keycode_to_key_shift[keycode_normal(0, 12)] = Key::BackTick;
		g_keycode_to_key_shift[keycode_normal(0, 13)] = Key::Backspace;
		g_keycode_to_key_shift[keycode_normal(1,  0)] = Key::Tab;
		g_keycode_to_key_shift[keycode_normal(1,  1)] = Key::Q;
		g_keycode_to_key_shift[keycode_normal(1,  2)] = Key::W;
		g_keycode_to_key_shift[keycode_normal(1,  3)] = Key::E;
		g_keycode_to_key_shift[keycode_normal(1,  4)] = Key::R;
		g_keycode_to_key_shift[keycode_normal(1,  5)] = Key::T;
		g_keycode_to_key_shift[keycode_normal(1,  6)] = Key::Y;
		g_keycode_to_key_shift[keycode_normal(1,  7)] = Key::U;
		g_keycode_to_key_shift[keycode_normal(1,  8)] = Key::I;
		g_keycode_to_key_shift[keycode_normal(1,  9)] = Key::O;
		g_keycode_to_key_shift[keycode_normal(1, 10)] = Key::P;
		g_keycode_to_key_shift[keycode_normal(1, 11)] = Key::A_Ring;
		g_keycode_to_key_shift[keycode_normal(1, 12)] = Key::Caret;
		g_keycode_to_key_shift[keycode_normal(2,  0)] = Key::CapsLock;
		g_keycode_to_key_shift[keycode_normal(2,  1)] = Key::A;
		g_keycode_to_key_shift[keycode_normal(2,  2)] = Key::S;
		g_keycode_to_key_shift[keycode_normal(2,  3)] = Key::D;
		g_keycode_to_key_shift[keycode_normal(2,  4)] = Key::F;
		g_keycode_to_key_shift[keycode_normal(2,  5)] = Key::G;
		g_keycode_to_key_shift[keycode_normal(2,  6)] = Key::H;
		g_keycode_to_key_shift[keycode_normal(2,  7)] = Key::J;
		g_keycode_to_key_shift[keycode_normal(2,  8)] = Key::K;
		g_keycode_to_key_shift[keycode_normal(2,  9)] = Key::L;
		g_keycode_to_key_shift[keycode_normal(2, 10)] = Key::O_Umlaut;
		g_keycode_to_key_shift[keycode_normal(2, 11)] = Key::A_Umlaut;
		g_keycode_to_key_shift[keycode_normal(2, 12)] = Key::Asterix;
		g_keycode_to_key_shift[keycode_normal(2, 13)] = Key::Enter;
		g_keycode_to_key_shift[keycode_normal(3,  0)] = Key::LeftShift;
		g_keycode_to_key_shift[keycode_normal(3,  1)] = Key::GreaterThan;
		g_keycode_to_key_shift[keycode_normal(3,  2)] = Key::Z;
		g_keycode_to_key_shift[keycode_normal(3,  3)] = Key::X;
		g_keycode_to_key_shift[keycode_normal(3,  4)] = Key::C;
		g_keycode_to_key_shift[keycode_normal(3,  5)] = Key::V;
		g_keycode_to_key_shift[keycode_normal(3,  6)] = Key::B;
		g_keycode_to_key_shift[keycode_normal(3,  7)] = Key::N;
		g_keycode_to_key_shift[keycode_normal(3,  8)] = Key::M;
		g_keycode_to_key_shift[keycode_normal(3,  9)] = Key::Semicolon;
		g_keycode_to_key_shift[keycode_normal(3, 10)] = Key::Colon;
		g_keycode_to_key_shift[keycode_normal(3, 11)] = Key::Underscore;
		g_keycode_to_key_shift[keycode_normal(3, 12)] = Key::RightShift;
		g_keycode_to_key_shift[keycode_normal(4,  0)] = Key::LeftCtrl;
		g_keycode_to_key_shift[keycode_normal(4,  1)] = Key::Super;
		g_keycode_to_key_shift[keycode_normal(4,  2)] = Key::LeftAlt;
		g_keycode_to_key_shift[keycode_normal(4,  3)] = Key::Space;
		g_keycode_to_key_shift[keycode_normal(4,  4)] = Key::RightAlt;
		g_keycode_to_key_shift[keycode_normal(4,  5)] = Key::RightCtrl;

		g_keycode_to_key_shift[keycode_numpad(0, 0)] = Key::NumLock;
		g_keycode_to_key_shift[keycode_numpad(0, 1)] = Key::NumpadDivide;
		g_keycode_to_key_shift[keycode_numpad(0, 2)] = Key::NumpadMultiply;
		g_keycode_to_key_shift[keycode_numpad(0, 3)] = Key::NumpadMinus;
		g_keycode_to_key_shift[keycode_numpad(1, 0)] = Key::Numpad7;
		g_keycode_to_key_shift[keycode_numpad(1, 1)] = Key::Numpad8;
		g_keycode_to_key_shift[keycode_numpad(1, 2)] = Key::Numpad9;
		g_keycode_to_key_shift[keycode_numpad(1, 3)] = Key::NumpadPlus;
		g_keycode_to_key_shift[keycode_numpad(2, 0)] = Key::Numpad4;
		g_keycode_to_key_shift[keycode_numpad(2, 1)] = Key::Numpad5;
		g_keycode_to_key_shift[keycode_numpad(2, 2)] = Key::Numpad6;
		g_keycode_to_key_shift[keycode_numpad(3, 0)] = Key::Numpad1;
		g_keycode_to_key_shift[keycode_numpad(3, 1)] = Key::Numpad2;
		g_keycode_to_key_shift[keycode_numpad(3, 2)] = Key::Numpad3;
		g_keycode_to_key_shift[keycode_numpad(3, 3)] = Key::NumpadEnter;
		g_keycode_to_key_shift[keycode_numpad(4, 0)] = Key::Numpad0;
		g_keycode_to_key_shift[keycode_numpad(4, 1)] = Key::NumpadDecimal;

		g_keycode_to_key_shift[keycode_function( 0)] = Key::Escape;
		g_keycode_to_key_shift[keycode_function( 1)] = Key::F1;
		g_keycode_to_key_shift[keycode_function( 2)] = Key::F2;
		g_keycode_to_key_shift[keycode_function( 3)] = Key::F3;
		g_keycode_to_key_shift[keycode_function( 4)] = Key::F4;
		g_keycode_to_key_shift[keycode_function( 5)] = Key::F5;
		g_keycode_to_key_shift[keycode_function( 6)] = Key::F6;
		g_keycode_to_key_shift[keycode_function( 7)] = Key::F7;
		g_keycode_to_key_shift[keycode_function( 8)] = Key::F8;
		g_keycode_to_key_shift[keycode_function( 9)] = Key::F9;
		g_keycode_to_key_shift[keycode_function(10)] = Key::F10;
		g_keycode_to_key_shift[keycode_function(11)] = Key::F11;
		g_keycode_to_key_shift[keycode_function(12)] = Key::F12;
		g_keycode_to_key_shift[keycode_function(13)] = Key::Insert;
		g_keycode_to_key_shift[keycode_function(14)] = Key::PrintScreen;
		g_keycode_to_key_shift[keycode_function(15)] = Key::Delete;
		g_keycode_to_key_shift[keycode_function(16)] = Key::Home;
		g_keycode_to_key_shift[keycode_function(17)] = Key::End;
		g_keycode_to_key_shift[keycode_function(18)] = Key::PageUp;
		g_keycode_to_key_shift[keycode_function(19)] = Key::PageDown;
		g_keycode_to_key_shift[keycode_function(20)] = Key::ScrollLock;

		// Arrow keys
		g_keycode_to_key_shift[keycode_normal(5, 0)] = Key::ArrowUp;
		g_keycode_to_key_shift[keycode_normal(5, 1)] = Key::ArrowLeft;
		g_keycode_to_key_shift[keycode_normal(5, 2)] = Key::ArrowDown;
		g_keycode_to_key_shift[keycode_normal(5, 3)] = Key::ArrowRight;
	}

	static void initialize_fi_altgr()
	{
		g_keycode_to_key_altgr[keycode_normal(0,  0)] = Key::Section;
		g_keycode_to_key_altgr[keycode_normal(0,  1)] = Key::None;
		g_keycode_to_key_altgr[keycode_normal(0,  2)] = Key::AtSign;
		g_keycode_to_key_altgr[keycode_normal(0,  3)] = Key::Pound;
		g_keycode_to_key_altgr[keycode_normal(0,  4)] = Key::Dollar;
		g_keycode_to_key_altgr[keycode_normal(0,  5)] = Key::None;
		g_keycode_to_key_altgr[keycode_normal(0,  6)] = Key::None;
		g_keycode_to_key_altgr[keycode_normal(0,  7)] = Key::OpenCurlyBracket;
		g_keycode_to_key_altgr[keycode_normal(0,  8)] = Key::OpenSquareBracket;
		g_keycode_to_key_altgr[keycode_normal(0,  9)] = Key::CloseSquareBracket;
		g_keycode_to_key_altgr[keycode_normal(0, 10)] = Key::CloseCurlyBracket;
		g_keycode_to_key_altgr[keycode_normal(0, 11)] = Key::BackSlash;
		g_keycode_to_key_altgr[keycode_normal(0, 12)] = Key::Cedilla;
		g_keycode_to_key_altgr[keycode_normal(0, 13)] = Key::Backspace;
		g_keycode_to_key_altgr[keycode_normal(1,  0)] = Key::Tab;
		g_keycode_to_key_altgr[keycode_normal(1,  1)] = Key::Q;
		g_keycode_to_key_altgr[keycode_normal(1,  2)] = Key::W;
		g_keycode_to_key_altgr[keycode_normal(1,  3)] = Key::Euro;
		g_keycode_to_key_altgr[keycode_normal(1,  4)] = Key::R;
		g_keycode_to_key_altgr[keycode_normal(1,  5)] = Key::T;
		g_keycode_to_key_altgr[keycode_normal(1,  6)] = Key::Y;
		g_keycode_to_key_altgr[keycode_normal(1,  7)] = Key::U;
		g_keycode_to_key_altgr[keycode_normal(1,  8)] = Key::I;
		g_keycode_to_key_altgr[keycode_normal(1,  9)] = Key::O;
		g_keycode_to_key_altgr[keycode_normal(1, 10)] = Key::P;
		g_keycode_to_key_altgr[keycode_normal(1, 11)] = Key::A_Ring;
		g_keycode_to_key_altgr[keycode_normal(1, 12)] = Key::Tilde;
		g_keycode_to_key_altgr[keycode_normal(2,  0)] = Key::CapsLock;
		g_keycode_to_key_altgr[keycode_normal(2,  1)] = Key::A;
		g_keycode_to_key_altgr[keycode_normal(2,  2)] = Key::S;
		g_keycode_to_key_altgr[keycode_normal(2,  3)] = Key::D;
		g_keycode_to_key_altgr[keycode_normal(2,  4)] = Key::F;
		g_keycode_to_key_altgr[keycode_normal(2,  5)] = Key::G;
		g_keycode_to_key_altgr[keycode_normal(2,  6)] = Key::H;
		g_keycode_to_key_altgr[keycode_normal(2,  7)] = Key::J;
		g_keycode_to_key_altgr[keycode_normal(2,  8)] = Key::K;
		g_keycode_to_key_altgr[keycode_normal(2,  9)] = Key::L;
		g_keycode_to_key_altgr[keycode_normal(2, 10)] = Key::O_Umlaut;
		g_keycode_to_key_altgr[keycode_normal(2, 11)] = Key::A_Umlaut;
		g_keycode_to_key_altgr[keycode_normal(2, 12)] = Key::None;
		g_keycode_to_key_altgr[keycode_normal(2, 13)] = Key::Enter;
		g_keycode_to_key_altgr[keycode_normal(3,  0)] = Key::LeftShift;
		g_keycode_to_key_altgr[keycode_normal(3,  1)] = Key::Pipe;
		g_keycode_to_key_altgr[keycode_normal(3,  2)] = Key::Z;
		g_keycode_to_key_altgr[keycode_normal(3,  3)] = Key::X;
		g_keycode_to_key_altgr[keycode_normal(3,  4)] = Key::C;
		g_keycode_to_key_altgr[keycode_normal(3,  5)] = Key::V;
		g_keycode_to_key_altgr[keycode_normal(3,  6)] = Key::B;
		g_keycode_to_key_altgr[keycode_normal(3,  7)] = Key::N;
		g_keycode_to_key_altgr[keycode_normal(3,  8)] = Key::M;
		g_keycode_to_key_altgr[keycode_normal(3,  9)] = Key::None;
		g_keycode_to_key_altgr[keycode_normal(3, 10)] = Key::None;
		g_keycode_to_key_altgr[keycode_normal(3, 11)] = Key::None;
		g_keycode_to_key_altgr[keycode_normal(3, 12)] = Key::RightShift;
		g_keycode_to_key_altgr[keycode_normal(4,  0)] = Key::LeftCtrl;
		g_keycode_to_key_altgr[keycode_normal(4,  1)] = Key::Super;
		g_keycode_to_key_altgr[keycode_normal(4,  2)] = Key::LeftAlt;
		g_keycode_to_key_altgr[keycode_normal(4,  3)] = Key::Space;
		g_keycode_to_key_altgr[keycode_normal(4,  4)] = Key::RightAlt;
		g_keycode_to_key_altgr[keycode_normal(4,  5)] = Key::RightCtrl;

		g_keycode_to_key_altgr[keycode_numpad(0, 0)] = Key::NumLock;
		g_keycode_to_key_altgr[keycode_numpad(0, 1)] = Key::NumpadDivide;
		g_keycode_to_key_altgr[keycode_numpad(0, 2)] = Key::NumpadMultiply;
		g_keycode_to_key_altgr[keycode_numpad(0, 3)] = Key::NumpadMinus;
		g_keycode_to_key_altgr[keycode_numpad(1, 0)] = Key::Numpad7;
		g_keycode_to_key_altgr[keycode_numpad(1, 1)] = Key::Numpad8;
		g_keycode_to_key_altgr[keycode_numpad(1, 2)] = Key::Numpad9;
		g_keycode_to_key_altgr[keycode_numpad(1, 3)] = Key::NumpadPlus;
		g_keycode_to_key_altgr[keycode_numpad(2, 0)] = Key::Numpad4;
		g_keycode_to_key_altgr[keycode_numpad(2, 1)] = Key::Numpad5;
		g_keycode_to_key_altgr[keycode_numpad(2, 2)] = Key::Numpad6;
		g_keycode_to_key_altgr[keycode_numpad(3, 0)] = Key::Numpad1;
		g_keycode_to_key_altgr[keycode_numpad(3, 1)] = Key::Numpad2;
		g_keycode_to_key_altgr[keycode_numpad(3, 2)] = Key::Numpad3;
		g_keycode_to_key_altgr[keycode_numpad(3, 3)] = Key::NumpadEnter;
		g_keycode_to_key_altgr[keycode_numpad(4, 0)] = Key::Numpad0;
		g_keycode_to_key_altgr[keycode_numpad(4, 1)] = Key::NumpadDecimal;

		g_keycode_to_key_altgr[keycode_function( 0)] = Key::Escape;
		g_keycode_to_key_altgr[keycode_function( 1)] = Key::F1;
		g_keycode_to_key_altgr[keycode_function( 2)] = Key::F2;
		g_keycode_to_key_altgr[keycode_function( 3)] = Key::F3;
		g_keycode_to_key_altgr[keycode_function( 4)] = Key::F4;
		g_keycode_to_key_altgr[keycode_function( 5)] = Key::F5;
		g_keycode_to_key_altgr[keycode_function( 6)] = Key::F6;
		g_keycode_to_key_altgr[keycode_function( 7)] = Key::F7;
		g_keycode_to_key_altgr[keycode_function( 8)] = Key::F8;
		g_keycode_to_key_altgr[keycode_function( 9)] = Key::F9;
		g_keycode_to_key_altgr[keycode_function(10)] = Key::F10;
		g_keycode_to_key_altgr[keycode_function(11)] = Key::F11;
		g_keycode_to_key_altgr[keycode_function(12)] = Key::F12;
		g_keycode_to_key_altgr[keycode_function(13)] = Key::Insert;
		g_keycode_to_key_altgr[keycode_function(14)] = Key::PrintScreen;
		g_keycode_to_key_altgr[keycode_function(15)] = Key::Delete;
		g_keycode_to_key_altgr[keycode_function(16)] = Key::Home;
		g_keycode_to_key_altgr[keycode_function(17)] = Key::End;
		g_keycode_to_key_altgr[keycode_function(18)] = Key::PageUp;
		g_keycode_to_key_altgr[keycode_function(19)] = Key::PageDown;
		g_keycode_to_key_altgr[keycode_function(20)] = Key::ScrollLock;

		// Arrow keys
		g_keycode_to_key_altgr[keycode_normal(5, 0)] = Key::ArrowUp;
		g_keycode_to_key_altgr[keycode_normal(5, 1)] = Key::ArrowLeft;
		g_keycode_to_key_altgr[keycode_normal(5, 2)] = Key::ArrowDown;
		g_keycode_to_key_altgr[keycode_normal(5, 3)] = Key::ArrowRight;
	}

}
