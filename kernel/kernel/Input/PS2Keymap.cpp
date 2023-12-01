#include <kernel/Input/PS2Keymap.h>

namespace Kernel::Input
{

	PS2Keymap::PS2Keymap()
	{
		MUST(m_normal_keymap.resize(0xFF, Key::Invalid));
		m_normal_keymap[0x01] = Key::F9;
		m_normal_keymap[0x03] = Key::F5;
		m_normal_keymap[0x04] = Key::F3;
		m_normal_keymap[0x05] = Key::F1;
		m_normal_keymap[0x06] = Key::F2;
		m_normal_keymap[0x07] = Key::F11;
		m_normal_keymap[0x09] = Key::F10;
		m_normal_keymap[0x0A] = Key::F8;
		m_normal_keymap[0x0B] = Key::F6;
		m_normal_keymap[0x0C] = Key::F4;
		m_normal_keymap[0x0D] = Key::Tab;
		m_normal_keymap[0x0E] = Key::Section;
		m_normal_keymap[0x11] = Key::Alt;
		m_normal_keymap[0x12] = Key::LeftShift;
		m_normal_keymap[0x14] = Key::LeftCtrl;
		m_normal_keymap[0x15] = Key::Q;
		m_normal_keymap[0x16] = Key::_1;
		m_normal_keymap[0x1A] = Key::Z;
		m_normal_keymap[0x1B] = Key::S;
		m_normal_keymap[0x1C] = Key::A;
		m_normal_keymap[0x1D] = Key::W;
		m_normal_keymap[0x1E] = Key::_2;
		m_normal_keymap[0x21] = Key::C;
		m_normal_keymap[0x22] = Key::X;
		m_normal_keymap[0x23] = Key::D;
		m_normal_keymap[0x24] = Key::E;
		m_normal_keymap[0x25] = Key::_4;
		m_normal_keymap[0x26] = Key::_3;
		m_normal_keymap[0x29] = Key::Space;
		m_normal_keymap[0x2A] = Key::V;
		m_normal_keymap[0x2B] = Key::F;
		m_normal_keymap[0x2C] = Key::T;
		m_normal_keymap[0x2D] = Key::R;
		m_normal_keymap[0x2E] = Key::_5;
		m_normal_keymap[0x31] = Key::N;
		m_normal_keymap[0x32] = Key::B;
		m_normal_keymap[0x33] = Key::H;
		m_normal_keymap[0x34] = Key::G;
		m_normal_keymap[0x35] = Key::Y;
		m_normal_keymap[0x36] = Key::_6;
		m_normal_keymap[0x3A] = Key::M;
		m_normal_keymap[0x3B] = Key::J;
		m_normal_keymap[0x3C] = Key::U;
		m_normal_keymap[0x3D] = Key::_7;
		m_normal_keymap[0x3E] = Key::_8;
		m_normal_keymap[0x41] = Key::Comma;
		m_normal_keymap[0x42] = Key::K;
		m_normal_keymap[0x43] = Key::I;
		m_normal_keymap[0x44] = Key::O;
		m_normal_keymap[0x45] = Key::_0;
		m_normal_keymap[0x46] = Key::_9;
		m_normal_keymap[0x49] = Key::Period;
		m_normal_keymap[0x4A] = Key::Hyphen;
		m_normal_keymap[0x4B] = Key::L;
		m_normal_keymap[0x4C] = Key::O_Umlaut;
		m_normal_keymap[0x4D] = Key::P;
		m_normal_keymap[0x4E] = Key::Plus;
		m_normal_keymap[0x52] = Key::A_Umlaut;
		m_normal_keymap[0x54] = Key::A_Ring;
		m_normal_keymap[0x55] = Key::Acute;
		m_normal_keymap[0x58] = Key::CapsLock;
		m_normal_keymap[0x59] = Key::RightShift;
		m_normal_keymap[0x59] = Key::RightShift;
		m_normal_keymap[0x5A] = Key::Enter;
		m_normal_keymap[0x5B] = Key::TwoDots;
		m_normal_keymap[0x5D] = Key::SingleQuote;
		m_normal_keymap[0x61] = Key::LessThan;
		m_normal_keymap[0x66] = Key::Backspace;
		m_normal_keymap[0x69] = Key::Numpad1;
		m_normal_keymap[0x6B] = Key::Numpad4;
		m_normal_keymap[0x6C] = Key::Numpad7;
		m_normal_keymap[0x70] = Key::Numpad0;
		m_normal_keymap[0x71] = Key::NumpadDecimal;
		m_normal_keymap[0x72] = Key::Numpad2;
		m_normal_keymap[0x73] = Key::Numpad5;
		m_normal_keymap[0x74] = Key::Numpad6;
		m_normal_keymap[0x75] = Key::Numpad8;
		m_normal_keymap[0x76] = Key::Escape;
		m_normal_keymap[0x77] = Key::NumLock;
		m_normal_keymap[0x78] = Key::F11;
		m_normal_keymap[0x79] = Key::NumpadPlus;
		m_normal_keymap[0x7A] = Key::Numpad3;
		m_normal_keymap[0x7B] = Key::NumpadMinus;
		m_normal_keymap[0x7C] = Key::NumpadMultiply;
		m_normal_keymap[0x7D] = Key::Numpad9;
		m_normal_keymap[0x83] = Key::F7;

		MUST(m_shift_keymap.resize(0xFF, Key::Invalid));
		m_shift_keymap[0x01] = Key::F9;
		m_shift_keymap[0x03] = Key::F5;
		m_shift_keymap[0x04] = Key::F3;
		m_shift_keymap[0x05] = Key::F1;
		m_shift_keymap[0x06] = Key::F2;
		m_shift_keymap[0x07] = Key::F11;
		m_shift_keymap[0x09] = Key::F10;
		m_shift_keymap[0x0A] = Key::F8;
		m_shift_keymap[0x0B] = Key::F6;
		m_shift_keymap[0x0C] = Key::F4;
		m_shift_keymap[0x0D] = Key::Tab;
		m_shift_keymap[0x0E] = Key::Half;
		m_shift_keymap[0x11] = Key::Alt;
		m_shift_keymap[0x12] = Key::LeftShift;
		m_shift_keymap[0x14] = Key::LeftCtrl;
		m_shift_keymap[0x15] = Key::Q;
		m_shift_keymap[0x16] = Key::ExclamationMark;
		m_shift_keymap[0x1A] = Key::Z;
		m_shift_keymap[0x1B] = Key::S;
		m_shift_keymap[0x1C] = Key::A;
		m_shift_keymap[0x1D] = Key::W;
		m_shift_keymap[0x1E] = Key::DoubleQuote;
		m_shift_keymap[0x21] = Key::C;
		m_shift_keymap[0x22] = Key::X;
		m_shift_keymap[0x23] = Key::D;
		m_shift_keymap[0x24] = Key::E;
		m_shift_keymap[0x25] = Key::Currency;
		m_shift_keymap[0x26] = Key::Hashtag;
		m_shift_keymap[0x29] = Key::Space;
		m_shift_keymap[0x2A] = Key::V;
		m_shift_keymap[0x2B] = Key::F;
		m_shift_keymap[0x2C] = Key::T;
		m_shift_keymap[0x2D] = Key::R;
		m_shift_keymap[0x2E] = Key::Percent;
		m_shift_keymap[0x31] = Key::N;
		m_shift_keymap[0x32] = Key::B;
		m_shift_keymap[0x33] = Key::H;
		m_shift_keymap[0x34] = Key::G;
		m_shift_keymap[0x35] = Key::Y;
		m_shift_keymap[0x36] = Key::Ampersand;
		m_shift_keymap[0x3A] = Key::M;
		m_shift_keymap[0x3B] = Key::J;
		m_shift_keymap[0x3C] = Key::U;
		m_shift_keymap[0x3D] = Key::Slash;
		m_shift_keymap[0x3E] = Key::OpenBracet;
		m_shift_keymap[0x41] = Key::Semicolon;
		m_shift_keymap[0x42] = Key::K;
		m_shift_keymap[0x43] = Key::I;
		m_shift_keymap[0x44] = Key::O;
		m_shift_keymap[0x45] = Key::Equals;
		m_shift_keymap[0x46] = Key::CloseBracet;
		m_shift_keymap[0x49] = Key::Colon;
		m_shift_keymap[0x4A] = Key::Underscore;
		m_shift_keymap[0x4B] = Key::L;
		m_shift_keymap[0x4C] = Key::O_Umlaut;
		m_shift_keymap[0x4D] = Key::P;
		m_shift_keymap[0x4E] = Key::QuestionMark;
		m_shift_keymap[0x52] = Key::A_Umlaut;
		m_shift_keymap[0x54] = Key::A_Ring;
		m_shift_keymap[0x55] = Key::BackTick;
		m_shift_keymap[0x58] = Key::CapsLock;
		m_shift_keymap[0x59] = Key::RightShift;
		m_shift_keymap[0x59] = Key::RightShift;
		m_shift_keymap[0x5A] = Key::Enter;
		m_shift_keymap[0x5B] = Key::Caret;
		m_shift_keymap[0x5D] = Key::Asterix;
		m_shift_keymap[0x61] = Key::GreaterThan;
		m_shift_keymap[0x66] = Key::Backspace;
		m_shift_keymap[0x69] = Key::Numpad1;
		m_shift_keymap[0x6B] = Key::Numpad4;
		m_shift_keymap[0x6C] = Key::Numpad7;
		m_shift_keymap[0x70] = Key::Numpad0;
		m_shift_keymap[0x71] = Key::NumpadDecimal;
		m_shift_keymap[0x72] = Key::Numpad2;
		m_shift_keymap[0x73] = Key::Numpad5;
		m_shift_keymap[0x74] = Key::Numpad6;
		m_shift_keymap[0x75] = Key::Numpad8;
		m_shift_keymap[0x76] = Key::Escape;
		m_shift_keymap[0x77] = Key::NumLock;
		m_shift_keymap[0x78] = Key::F11;
		m_shift_keymap[0x79] = Key::NumpadPlus;
		m_shift_keymap[0x7A] = Key::Numpad3;
		m_shift_keymap[0x7B] = Key::NumpadMinus;
		m_shift_keymap[0x7C] = Key::NumpadMultiply;
		m_shift_keymap[0x7D] = Key::Numpad9;
		m_shift_keymap[0x83] = Key::F7;

		MUST(m_altgr_keymap.resize(0xFF, Key::Invalid));
		m_altgr_keymap[0x01] = Key::F9;
		m_altgr_keymap[0x03] = Key::F5;
		m_altgr_keymap[0x04] = Key::F3;
		m_altgr_keymap[0x05] = Key::F1;
		m_altgr_keymap[0x06] = Key::F2;
		m_altgr_keymap[0x07] = Key::F11;
		m_altgr_keymap[0x09] = Key::F10;
		m_altgr_keymap[0x0A] = Key::F8;
		m_altgr_keymap[0x0B] = Key::F6;
		m_altgr_keymap[0x0C] = Key::F4;
		m_altgr_keymap[0x0D] = Key::Tab;
		m_altgr_keymap[0x0E] = Key::None;
		m_altgr_keymap[0x11] = Key::Alt;
		m_altgr_keymap[0x12] = Key::LeftShift;
		m_altgr_keymap[0x14] = Key::LeftCtrl;
		m_altgr_keymap[0x15] = Key::Q;
		m_altgr_keymap[0x16] = Key::None;
		m_altgr_keymap[0x1A] = Key::Z;
		m_altgr_keymap[0x1B] = Key::S;
		m_altgr_keymap[0x1C] = Key::A;
		m_altgr_keymap[0x1D] = Key::W;
		m_altgr_keymap[0x1E] = Key::AtSign;
		m_altgr_keymap[0x21] = Key::C;
		m_altgr_keymap[0x22] = Key::X;
		m_altgr_keymap[0x23] = Key::D;
		m_altgr_keymap[0x24] = Key::Euro;
		m_altgr_keymap[0x25] = Key::Dollar;
		m_altgr_keymap[0x26] = Key::Pound;
		m_altgr_keymap[0x29] = Key::Space;
		m_altgr_keymap[0x2A] = Key::V;
		m_altgr_keymap[0x2B] = Key::F;
		m_altgr_keymap[0x2C] = Key::T;
		m_altgr_keymap[0x2D] = Key::R;
		m_altgr_keymap[0x2E] = Key::None;
		m_altgr_keymap[0x31] = Key::N;
		m_altgr_keymap[0x32] = Key::B;
		m_altgr_keymap[0x33] = Key::H;
		m_altgr_keymap[0x34] = Key::G;
		m_altgr_keymap[0x35] = Key::Y;
		m_altgr_keymap[0x36] = Key::None;
		m_altgr_keymap[0x3A] = Key::M;
		m_altgr_keymap[0x3B] = Key::J;
		m_altgr_keymap[0x3C] = Key::U;
		m_altgr_keymap[0x3D] = Key::OpenCurlyBrace;
		m_altgr_keymap[0x3E] = Key::OpenBrace;
		m_altgr_keymap[0x41] = Key::None;
		m_altgr_keymap[0x42] = Key::K;
		m_altgr_keymap[0x43] = Key::I;
		m_altgr_keymap[0x44] = Key::O;
		m_altgr_keymap[0x45] = Key::CloseCurlyBrace;
		m_altgr_keymap[0x46] = Key::CloseBrace;
		m_altgr_keymap[0x49] = Key::None;
		m_altgr_keymap[0x4A] = Key::None;
		m_altgr_keymap[0x4B] = Key::L;
		m_altgr_keymap[0x4C] = Key::O_Umlaut;
		m_altgr_keymap[0x4D] = Key::P;
		m_altgr_keymap[0x4E] = Key::BackSlash;
		m_altgr_keymap[0x52] = Key::A_Umlaut;
		m_altgr_keymap[0x54] = Key::A_Ring;
		m_altgr_keymap[0x55] = Key::None;
		m_altgr_keymap[0x58] = Key::CapsLock;
		m_altgr_keymap[0x59] = Key::RightShift;
		m_altgr_keymap[0x59] = Key::RightShift;
		m_altgr_keymap[0x5A] = Key::Enter;
		m_altgr_keymap[0x5B] = Key::Tilde;
		m_altgr_keymap[0x5D] = Key::None;
		m_altgr_keymap[0x61] = Key::Pipe;
		m_altgr_keymap[0x66] = Key::Backspace;
		m_altgr_keymap[0x69] = Key::Numpad1;
		m_altgr_keymap[0x6B] = Key::Numpad4;
		m_altgr_keymap[0x6C] = Key::Numpad7;
		m_altgr_keymap[0x70] = Key::Numpad0;
		m_altgr_keymap[0x71] = Key::NumpadDecimal;
		m_altgr_keymap[0x72] = Key::Numpad2;
		m_altgr_keymap[0x73] = Key::Numpad5;
		m_altgr_keymap[0x74] = Key::Numpad6;
		m_altgr_keymap[0x75] = Key::Numpad8;
		m_altgr_keymap[0x76] = Key::Escape;
		m_altgr_keymap[0x77] = Key::NumLock;
		m_altgr_keymap[0x78] = Key::F11;
		m_altgr_keymap[0x79] = Key::NumpadPlus;
		m_altgr_keymap[0x7A] = Key::Numpad3;
		m_altgr_keymap[0x7B] = Key::NumpadMinus;
		m_altgr_keymap[0x7C] = Key::NumpadMultiply;
		m_altgr_keymap[0x7D] = Key::Numpad9;
		m_altgr_keymap[0x83] = Key::F7;

		MUST(m_extended_keymap.resize(0xFF, Key::Invalid));
		m_extended_keymap[0x11] = Key::AltGr;
		m_extended_keymap[0x14] = Key::RightCtrl;
		m_extended_keymap[0x15] = Key::MediaPrevious;
		m_extended_keymap[0x1F] = Key::Super;
		m_extended_keymap[0x21] = Key::VolumeUp;
		m_extended_keymap[0x23] = Key::VolumeMute;
		m_extended_keymap[0x2B] = Key::Calculator;
		m_extended_keymap[0x32] = Key::VolumeDown;
		m_extended_keymap[0x34] = Key::MediaPlayPause;
		m_extended_keymap[0x3B] = Key::MediaStop;
		m_extended_keymap[0x4A] = Key::NumpadDivide;
		m_extended_keymap[0x4D] = Key::MediaNext;
		m_extended_keymap[0x5A] = Key::NumpadEnter;
		m_extended_keymap[0x69] = Key::End;
		m_extended_keymap[0x6B] = Key::ArrowLeft;
		m_extended_keymap[0x6C] = Key::Home;
		m_extended_keymap[0x70] = Key::Insert;
		m_extended_keymap[0x71] = Key::Delete;
		m_extended_keymap[0x72] = Key::ArrowDown;
		m_extended_keymap[0x74] = Key::ArrowRight;
		m_extended_keymap[0x75] = Key::ArrowUp;
		m_extended_keymap[0x7A] = Key::PageUp;
		m_extended_keymap[0x7D] = Key::PageDown;
	}

	Key PS2Keymap::key_for_scancode_and_modifiers(uint32_t scancode, uint8_t modifiers)
	{
		bool extended = scancode & 0x80000000;
		scancode &= 0x7FFFFFFF;

		KeyEvent dummy;
		dummy.modifier = modifiers;
		auto& keymap =	extended      ? m_extended_keymap :
						dummy.shift() ? m_shift_keymap :
						dummy.altgr() ? m_altgr_keymap :
										m_normal_keymap;

		if (scancode >= keymap.size())
			return Key::Invalid;

		Key key = keymap[scancode];

		if (!dummy.num_lock() || dummy.shift() || dummy.ctrl() || dummy.alt())
		{
			switch (key)
			{
				case Key::Numpad0:
					key = Key::Insert;
					break;
				case Key::Numpad1:
					key = Key::End;
					break;
				case Key::Numpad2:
					key = Key::ArrowDown;
					break;
				case Key::Numpad3:
					key = Key::PageDown;
					break;
				case Key::Numpad4:
					key = Key::ArrowLeft;
					break;
				case Key::Numpad5:
					key = Key::None;
					break;
				case Key::Numpad6:
					key = Key::ArrowRight;
					break;
				case Key::Numpad7:
					key = Key::Home;
					break;
				case Key::Numpad8:
					key = Key::ArrowUp;
					break;
				case Key::Numpad9:
					key = Key::PageUp;
					break;
				case Key::NumpadDecimal:
					key = Key::Delete;
					break;
				default:
					break;
			}
		}

		return key;
	}

}