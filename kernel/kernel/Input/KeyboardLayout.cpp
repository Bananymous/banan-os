#include <BAN/HashMap.h>
#include <kernel/CriticalScope.h>
#include <kernel/FS/VirtualFileSystem.h>
#include <kernel/Input/KeyboardLayout.h>

#include <ctype.h>

namespace Kernel::Input
{

	struct StringViewLower
	{
		BAN::StringView value;

		StringViewLower(BAN::StringView sv)
			: value(sv)
		{ }

		bool operator==(const StringViewLower& other) const
		{
			if (value.size() != other.value.size())
				return false;
			for (size_t i = 0; i < value.size(); i++)
				if (tolower(value[i]) != tolower(other.value[i]))
					return false;
			return true;
		}
	};

	struct StringViewLowerHash
	{
		BAN::hash_t operator()(const StringViewLower& value) const
		{
			constexpr BAN::hash_t FNV_offset_basis = 0x811c9dc5;
			constexpr BAN::hash_t FNV_prime = 0x01000193;

			BAN::hash_t hash = FNV_offset_basis;
			for (size_t i = 0; i < value.value.size(); i++)
			{
				hash *= FNV_prime;
				hash ^= (uint8_t)tolower(value.value[i]);
			}

			return hash;
		}
	};



	static BAN::UniqPtr<KeyboardLayout> s_instance;

	BAN::ErrorOr<void> KeyboardLayout::initialize()
	{
		ASSERT(!s_instance);
		s_instance = TRY(BAN::UniqPtr<KeyboardLayout>::create());
		return {};
	}

	KeyboardLayout& KeyboardLayout::get()
	{
		ASSERT(s_instance);
		return *s_instance;
	}

	KeyboardLayout::KeyboardLayout()
	{
		for (auto& key : m_keycode_to_key_normal)
			key = Key::None;
		for (auto& key : m_keycode_to_key_shift)
			key = Key::None;
		for (auto& key : m_keycode_to_key_altgr)
			key = Key::None;
	}

	Key KeyboardLayout::get_key_from_event(KeyEvent event)
	{
		if (event.shift())
			return m_keycode_to_key_shift[event.keycode];
		if (event.ralt())
			return m_keycode_to_key_altgr[event.keycode];
		return m_keycode_to_key_normal[event.keycode];
	}

	static BAN::Optional<uint8_t> parse_keycode(BAN::StringView str)
	{
		if (str.size() > 3)
			return {};
		uint16_t keycode = 0;
		for (char c : str)
		{
			if (!isdigit(c))
				return {};
			keycode = (keycode * 10) + (c - '0');
		}
		if (keycode >= 0xFF)
			return {};
		return keycode;
	}

	static BAN::HashMap<StringViewLower, Key, StringViewLowerHash> s_name_to_key;
	static BAN::ErrorOr<void> initialize_name_to_key();

	static BAN::Optional<Key> parse_key(BAN::StringView name)
	{
		if (s_name_to_key.contains(name))
			return s_name_to_key[name];
		return {};
	}

	static BAN::ErrorOr<BAN::Vector<BAN::String>> load_keymap_lines_and_parse_includes(BAN::StringView path)
	{
		auto file = TRY(VirtualFileSystem::get().file_from_absolute_path({ 0, 0, 0, 0 }, path, 0));

		BAN::String file_data;
		TRY(file_data.resize(file.inode->size()));
		TRY(file.inode->read(0, BAN::ByteSpan { reinterpret_cast<uint8_t*>(file_data.data()), file_data.size() }));

		BAN::Vector<BAN::String> result;

		auto lines = TRY(file_data.sv().split('\n'));
		for (auto line : lines)
		{
			auto parts = TRY(line.split([](char c) -> bool { return isspace(c); }));
			if (parts.empty() || parts.front().front() == '#')
				continue;

			if (parts.front() == "include"sv)
			{
				if (parts.size() != 2)
				{
					dprintln("Invalid modifier instruction in keymap '{}'", line);
					dprintln("  format: include \"PATH\"");
					return BAN::Error::from_errno(EINVAL);
				}

				if (parts[1].size() < 2 || parts[1].front() != '"' || parts[1].back() != '"')
				{
					dprintln("Invalid modifier instruction in keymap '{}'", line);
					dprintln("  format: include \"PATH\"");
					return BAN::Error::from_errno(EINVAL);
				}
				parts[1] = parts[1].substring(1, parts[1].size() - 2);

				BAN::String include_path;
				TRY(include_path.append(file.canonical_path));
				ASSERT(include_path.sv().contains('/'));
				while (include_path.back() != '/')
					include_path.pop_back();
				TRY(include_path.append(parts[1]));

				auto new_lines = TRY(load_keymap_lines_and_parse_includes(include_path));
				TRY(result.reserve(result.size() + new_lines.size()));
				for (auto& line : new_lines)
					TRY(result.push_back(BAN::move(line)));
			}
			else
			{
				BAN::String line_str;
				TRY(line_str.append(line));
				TRY(result.push_back(BAN::move(line_str)));
			}
		}

		return result;
	}

	BAN::ErrorOr<void> KeyboardLayout::load_from_file(BAN::StringView path)
	{
		if (s_name_to_key.empty())
			TRY(initialize_name_to_key());

		auto new_layout = TRY(BAN::UniqPtr<KeyboardLayout>::create());

		bool shift_is_mod = false;
		bool altgr_is_mod = false;

		auto lines = TRY(load_keymap_lines_and_parse_includes(path));
		for (const auto& line : lines)
		{
			auto parts = TRY(line.sv().split([](char c) -> bool { return isspace(c); }));
			if (parts.empty() || parts.front().front() == '#')
				continue;

			if (parts.size() == 1)
			{
				dprintln("Invalid line in keymap '{}'", line);
				dprintln("  format: KEYCODE KEY [MODIFIER=KEY]...");
				dprintln("  format: mod MODIFIER");
				dprintln("  format: include \"PATH\"");
				return BAN::Error::from_errno(EINVAL);
			}

			if (parts.front() == "mod"sv)
			{
				if (parts.size() != 2)
				{
					dprintln("Invalid modifier instruction in keymap '{}'", line);
					dprintln("  format: mod MODIFIER");
					return BAN::Error::from_errno(EINVAL);
				}
				if (parts[1] == "shift"sv)
					shift_is_mod = true;
				else if (parts[1] == "altgr"sv)
					altgr_is_mod = true;
				else
				{
					dprintln("Unrecognized modifier '{}'", parts[1]);
					return BAN::Error::from_errno(EINVAL);
				}
				continue;
			}

			auto keycode = parse_keycode(parts.front());
			if (!keycode.has_value())
			{
				dprintln("Invalid keycode '{}', keycode must number between [0, 0xFF[", parts.front());
				return BAN::Error::from_errno(EINVAL);
			}

			auto default_key = parse_key(parts[1]);
			if (!default_key.has_value())
			{
				dprintln("Unrecognized key '{}'", parts[1]);
				return BAN::Error::from_errno(EINVAL);
			}

			new_layout->m_keycode_to_key_normal[*keycode] = *default_key;
			new_layout->m_keycode_to_key_shift[*keycode] = *default_key;
			new_layout->m_keycode_to_key_altgr[*keycode] = *default_key;

			for (size_t i = 2; i < parts.size(); i++)
			{
				auto pair = TRY(parts[i].split('='));
				if (pair.size() != 2)
				{
					dprintln("Invalid modifier format '{}', modifier format: MODIFIRER=KEY", parts[i]);
					return BAN::Error::from_errno(EINVAL);
				}

				auto key = parse_key(pair.back());
				if (!key.has_value())
				{
					dprintln("Unrecognized key '{}'", pair.back());
					return BAN::Error::from_errno(EINVAL);
				}

				if (shift_is_mod && pair.front() == "shift"sv)
					new_layout->m_keycode_to_key_shift[*keycode] = *key;
				else if (altgr_is_mod && pair.front() == "altgr"sv)
					new_layout->m_keycode_to_key_altgr[*keycode] = *key;
				else
				{
					dprintln("Unrecognized modifier '{}'", pair.front());
					return BAN::Error::from_errno(EINVAL);
				}
			}
		}

		CriticalScope _;

		for (size_t i = 0; i < new_layout->m_keycode_to_key_normal.size(); i++)
			if (new_layout->m_keycode_to_key_normal[i] != Key::None)
				m_keycode_to_key_normal[i] = new_layout->m_keycode_to_key_normal[i];

		for (size_t i = 0; i < new_layout->m_keycode_to_key_shift.size(); i++)
			if (new_layout->m_keycode_to_key_shift[i] != Key::None)
				m_keycode_to_key_shift[i] = new_layout->m_keycode_to_key_shift[i];

		for (size_t i = 0; i < new_layout->m_keycode_to_key_altgr.size(); i++)
			if (new_layout->m_keycode_to_key_altgr[i] != Key::None)
				m_keycode_to_key_altgr[i] = new_layout->m_keycode_to_key_altgr[i];

		return {};
	}

	static BAN::ErrorOr<void> initialize_name_to_key()
	{
		ASSERT(s_name_to_key.empty());
		TRY(s_name_to_key.insert("A_Ring"sv,				Key::A_Ring));
		TRY(s_name_to_key.insert("A_Umlaut"sv,				Key::A_Umlaut));
		TRY(s_name_to_key.insert("A"sv,						Key::A));
		TRY(s_name_to_key.insert("Acute"sv,					Key::Acute));
		TRY(s_name_to_key.insert("AltGr"sv,					Key::AltGr));
		TRY(s_name_to_key.insert("Ampersand"sv,				Key::Ampersand));
		TRY(s_name_to_key.insert("ArrowDown"sv,				Key::ArrowDown));
		TRY(s_name_to_key.insert("ArrowLeft"sv,				Key::ArrowLeft));
		TRY(s_name_to_key.insert("ArrowRight"sv,			Key::ArrowRight));
		TRY(s_name_to_key.insert("ArrowUp"sv,				Key::ArrowUp));
		TRY(s_name_to_key.insert("Asterix"sv,				Key::Asterix));
		TRY(s_name_to_key.insert("AtSign"sv,				Key::AtSign));
		TRY(s_name_to_key.insert("B"sv,						Key::B));
		TRY(s_name_to_key.insert("BackSlash"sv,				Key::BackSlash));
		TRY(s_name_to_key.insert("Backspace"sv,				Key::Backspace));
		TRY(s_name_to_key.insert("BackTick"sv,				Key::BackTick));
		TRY(s_name_to_key.insert("BrokenBar"sv,				Key::BrokenBar));
		TRY(s_name_to_key.insert("C"sv,						Key::C));
		TRY(s_name_to_key.insert("Calculator"sv,			Key::Calculator));
		TRY(s_name_to_key.insert("CapsLock"sv,				Key::CapsLock));
		TRY(s_name_to_key.insert("Caret"sv,					Key::Caret));
		TRY(s_name_to_key.insert("Cedilla"sv,				Key::Cedilla));
		TRY(s_name_to_key.insert("CloseCurlyBracket"sv,		Key::CloseCurlyBracket));
		TRY(s_name_to_key.insert("CloseParenthesis"sv,		Key::CloseParenthesis));
		TRY(s_name_to_key.insert("CloseSquareBracket"sv,	Key::CloseSquareBracket));
		TRY(s_name_to_key.insert("Colon"sv,					Key::Colon));
		TRY(s_name_to_key.insert("Comma"sv,					Key::Comma));
		TRY(s_name_to_key.insert("Currency"sv,				Key::Currency));
		TRY(s_name_to_key.insert("D"sv,						Key::D));
		TRY(s_name_to_key.insert("Delete"sv,				Key::Delete));
		TRY(s_name_to_key.insert("Dollar"sv,				Key::Dollar));
		TRY(s_name_to_key.insert("DoubleQuote"sv,			Key::DoubleQuote));
		TRY(s_name_to_key.insert("E"sv,						Key::E));
		TRY(s_name_to_key.insert("End"sv,					Key::End));
		TRY(s_name_to_key.insert("Enter"sv,					Key::Enter));
		TRY(s_name_to_key.insert("Equals"sv,				Key::Equals));
		TRY(s_name_to_key.insert("Escape"sv,				Key::Escape));
		TRY(s_name_to_key.insert("Euro"sv,					Key::Euro));
		TRY(s_name_to_key.insert("Exclamation"sv,			Key::ExclamationMark));
		TRY(s_name_to_key.insert("ExclamationMark"sv,		Key::ExclamationMark));
		TRY(s_name_to_key.insert("F"sv,						Key::F));
		TRY(s_name_to_key.insert("F1"sv,					Key::F1));
		TRY(s_name_to_key.insert("F10"sv,					Key::F10));
		TRY(s_name_to_key.insert("F11"sv,					Key::F11));
		TRY(s_name_to_key.insert("F12"sv,					Key::F12));
		TRY(s_name_to_key.insert("F2"sv,					Key::F2));
		TRY(s_name_to_key.insert("F3"sv,					Key::F3));
		TRY(s_name_to_key.insert("F4"sv,					Key::F4));
		TRY(s_name_to_key.insert("F5"sv,					Key::F5));
		TRY(s_name_to_key.insert("F6"sv,					Key::F6));
		TRY(s_name_to_key.insert("F7"sv,					Key::F7));
		TRY(s_name_to_key.insert("F8"sv,					Key::F8));
		TRY(s_name_to_key.insert("F9"sv,					Key::F9));
		TRY(s_name_to_key.insert("G"sv,						Key::G));
		TRY(s_name_to_key.insert("GreaterThan"sv,			Key::GreaterThan));
		TRY(s_name_to_key.insert("H"sv,						Key::H));
		TRY(s_name_to_key.insert("Half"sv,					Key::Half));
		TRY(s_name_to_key.insert("Hashtag"sv,				Key::Hashtag));
		TRY(s_name_to_key.insert("Home"sv,					Key::Home));
		TRY(s_name_to_key.insert("Hyphen"sv,				Key::Hyphen));
		TRY(s_name_to_key.insert("I"sv,						Key::I));
		TRY(s_name_to_key.insert("Insert"sv,				Key::Insert));
		TRY(s_name_to_key.insert("J"sv,						Key::J));
		TRY(s_name_to_key.insert("K"sv,						Key::K));
		TRY(s_name_to_key.insert("Key0"sv,					Key::_0));
		TRY(s_name_to_key.insert("Key1"sv,					Key::_1));
		TRY(s_name_to_key.insert("Key2"sv,					Key::_2));
		TRY(s_name_to_key.insert("Key3"sv,					Key::_3));
		TRY(s_name_to_key.insert("Key4"sv,					Key::_4));
		TRY(s_name_to_key.insert("Key5"sv,					Key::_5));
		TRY(s_name_to_key.insert("Key6"sv,					Key::_6));
		TRY(s_name_to_key.insert("Key7"sv,					Key::_7));
		TRY(s_name_to_key.insert("Key8"sv,					Key::_8));
		TRY(s_name_to_key.insert("Key9"sv,					Key::_9));
		TRY(s_name_to_key.insert("L"sv,						Key::L));
		TRY(s_name_to_key.insert("LAlt"sv,					Key::LeftAlt));
		TRY(s_name_to_key.insert("LControl"sv,				Key::LeftCtrl));
		TRY(s_name_to_key.insert("LeftAlt"sv,				Key::LeftAlt));
		TRY(s_name_to_key.insert("LeftControl"sv,			Key::LeftCtrl));
		TRY(s_name_to_key.insert("LeftShift"sv,				Key::LeftShift));
		TRY(s_name_to_key.insert("LessThan"sv,				Key::LessThan));
		TRY(s_name_to_key.insert("LShift"sv,				Key::LeftShift));
		TRY(s_name_to_key.insert("M"sv,						Key::M));
		TRY(s_name_to_key.insert("MediaNext"sv,				Key::MediaNext));
		TRY(s_name_to_key.insert("MediaPlayPause"sv,		Key::MediaPlayPause));
		TRY(s_name_to_key.insert("MediaPrevious"sv,			Key::MediaPrevious));
		TRY(s_name_to_key.insert("MediaStop"sv,				Key::MediaStop));
		TRY(s_name_to_key.insert("N"sv,						Key::N));
		TRY(s_name_to_key.insert("Negation"sv,				Key::Negation));
		TRY(s_name_to_key.insert("None"sv,					Key::None));
		TRY(s_name_to_key.insert("NumLock"sv,				Key::NumLock));
		TRY(s_name_to_key.insert("Numpad0"sv,				Key::Numpad0));
		TRY(s_name_to_key.insert("Numpad1"sv,				Key::Numpad1));
		TRY(s_name_to_key.insert("Numpad2"sv,				Key::Numpad2));
		TRY(s_name_to_key.insert("Numpad3"sv,				Key::Numpad3));
		TRY(s_name_to_key.insert("Numpad4"sv,				Key::Numpad4));
		TRY(s_name_to_key.insert("Numpad5"sv,				Key::Numpad5));
		TRY(s_name_to_key.insert("Numpad6"sv,				Key::Numpad6));
		TRY(s_name_to_key.insert("Numpad7"sv,				Key::Numpad7));
		TRY(s_name_to_key.insert("Numpad8"sv,				Key::Numpad8));
		TRY(s_name_to_key.insert("Numpad9"sv,				Key::Numpad9));
		TRY(s_name_to_key.insert("NumpadDecimal"sv,			Key::NumpadDecimal));
		TRY(s_name_to_key.insert("NumpadDivide"sv,			Key::NumpadDivide));
		TRY(s_name_to_key.insert("NumpadEnter"sv,			Key::NumpadEnter));
		TRY(s_name_to_key.insert("NumpadMinus"sv,			Key::NumpadMinus));
		TRY(s_name_to_key.insert("NumpadMultiply"sv,		Key::NumpadMultiply));
		TRY(s_name_to_key.insert("NumpadPlus"sv,			Key::NumpadPlus));
		TRY(s_name_to_key.insert("O_Umlaut"sv,				Key::O_Umlaut));
		TRY(s_name_to_key.insert("O"sv,						Key::O));
		TRY(s_name_to_key.insert("OpenCurlyBracket"sv,		Key::OpenCurlyBracket));
		TRY(s_name_to_key.insert("OpenParenthesis"sv,		Key::OpenParenthesis));
		TRY(s_name_to_key.insert("OpenSquareBracket"sv,		Key::OpenSquareBracket));
		TRY(s_name_to_key.insert("P"sv,						Key::P));
		TRY(s_name_to_key.insert("PageDown"sv,				Key::PageDown));
		TRY(s_name_to_key.insert("PageUp"sv,				Key::PageUp));
		TRY(s_name_to_key.insert("Percent"sv,				Key::Percent));
		TRY(s_name_to_key.insert("Period"sv,				Key::Period));
		TRY(s_name_to_key.insert("Pipe"sv,					Key::Pipe));
		TRY(s_name_to_key.insert("Plus"sv,					Key::Plus));
		TRY(s_name_to_key.insert("Pound"sv,					Key::Pound));
		TRY(s_name_to_key.insert("PrintScreen"sv,			Key::PrintScreen));
		TRY(s_name_to_key.insert("Q"sv,						Key::Q));
		TRY(s_name_to_key.insert("Question"sv,				Key::QuestionMark));
		TRY(s_name_to_key.insert("QuestionMark"sv,			Key::QuestionMark));
		TRY(s_name_to_key.insert("R"sv,						Key::R));
		TRY(s_name_to_key.insert("RAlt"sv,					Key::RightAlt));
		TRY(s_name_to_key.insert("RControl"sv,				Key::RightCtrl));
		TRY(s_name_to_key.insert("RightAlt"sv,				Key::RightAlt));
		TRY(s_name_to_key.insert("RightControl"sv,			Key::RightCtrl));
		TRY(s_name_to_key.insert("RightShift"sv,			Key::RightShift));
		TRY(s_name_to_key.insert("RShift"sv,				Key::RightShift));
		TRY(s_name_to_key.insert("S"sv,						Key::S));
		TRY(s_name_to_key.insert("ScrollLock"sv,			Key::ScrollLock));
		TRY(s_name_to_key.insert("Section"sv,				Key::Section));
		TRY(s_name_to_key.insert("Semicolon"sv,				Key::Semicolon));
		TRY(s_name_to_key.insert("SingleQuote"sv,			Key::SingleQuote));
		TRY(s_name_to_key.insert("Slash"sv,					Key::Slash));
		TRY(s_name_to_key.insert("Space"sv,					Key::Space));
		TRY(s_name_to_key.insert("Super"sv,					Key::Super));
		TRY(s_name_to_key.insert("T"sv,						Key::T));
		TRY(s_name_to_key.insert("Tab"sv,					Key::Tab));
		TRY(s_name_to_key.insert("Tilde"sv,					Key::Tilde));
		TRY(s_name_to_key.insert("TwoDots"sv,				Key::TwoDots));
		TRY(s_name_to_key.insert("U"sv,						Key::U));
		TRY(s_name_to_key.insert("Underscore"sv,			Key::Underscore));
		TRY(s_name_to_key.insert("V"sv,						Key::V));
		TRY(s_name_to_key.insert("VolumeDown"sv,			Key::VolumeDown));
		TRY(s_name_to_key.insert("VolumeMute"sv,			Key::VolumeMute));
		TRY(s_name_to_key.insert("VolumeUp"sv,				Key::VolumeUp));
		TRY(s_name_to_key.insert("W"sv,						Key::W));
		TRY(s_name_to_key.insert("X"sv,						Key::X));
		TRY(s_name_to_key.insert("Y"sv,						Key::Y));
		TRY(s_name_to_key.insert("Z"sv,						Key::Z));
		return {};
	}

}
