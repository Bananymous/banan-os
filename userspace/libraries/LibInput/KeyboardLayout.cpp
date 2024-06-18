#include <BAN/Debug.h>
#include <BAN/HashMap.h>
#include <BAN/String.h>
#include <BAN/StringView.h>
#include <LibInput/KeyboardLayout.h>

#if __is_kernel
#include <kernel/FS/VirtualFileSystem.h>
#else
#include <fcntl.h>
#include <limits.h>
#include <sys/stat.h>
#endif

#include <ctype.h>

namespace LibInput
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

	KeyEvent KeyboardLayout::key_event_from_raw(RawKeyEvent event)
	{
		KeyEvent result;
		result.modifier = event.modifier;
		if (result.shift())
			result.key = m_keycode_to_key_shift[event.keycode];
		else if (result.ralt())
			result.key = m_keycode_to_key_altgr[event.keycode];
		else
			result.key = m_keycode_to_key_normal[event.keycode];
		return result;
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
		BAN::String file_data;
		BAN::String canonical_path;

#if __is_kernel
		{
			auto file = TRY(Kernel::VirtualFileSystem::get().file_from_absolute_path({ 0, 0, 0, 0 }, path, 0));
			TRY(file_data.resize(file.inode->size()));
			TRY(file.inode->read(0, BAN::ByteSpan { reinterpret_cast<uint8_t*>(file_data.data()), file_data.size() }));
			canonical_path = file.canonical_path;
		}
#else
		{
			char null_path[PATH_MAX];
			strncpy(null_path, path.data(), path.size());
			null_path[path.size()] = '\0';

			struct stat st;
			if (stat(null_path, &st) == -1)
				return BAN::Error::from_errno(errno);
			TRY(file_data.resize(st.st_size));
			int fd = open(null_path, O_RDONLY);
			if (fd == -1)
				return BAN::Error::from_errno(errno);
			ssize_t nread = read(fd, file_data.data(), st.st_size);
			close(fd);
			if (nread != st.st_size)
				return BAN::Error::from_errno(errno);
			MUST(canonical_path.append(path));
		}
#endif

		BAN::Vector<BAN::String> result;

		auto lines = TRY(file_data.sv().split('\n'));
		for (auto line : lines)
		{
			auto parts = TRY(line.split([](char c) -> bool { return isspace(c); }));
			if (parts.empty() || parts.front().front() == '#')
				continue;

			if (parts.front() == "include"_sv)
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
				TRY(include_path.append(canonical_path));
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

			if (parts.front() == "mod"_sv)
			{
				if (parts.size() != 2)
				{
					dprintln("Invalid modifier instruction in keymap '{}'", line);
					dprintln("  format: mod MODIFIER");
					return BAN::Error::from_errno(EINVAL);
				}
				if (parts[1] == "shift"_sv)
					shift_is_mod = true;
				else if (parts[1] == "altgr"_sv)
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

				if (shift_is_mod && pair.front() == "shift"_sv)
					new_layout->m_keycode_to_key_shift[*keycode] = *key;
				else if (altgr_is_mod && pair.front() == "altgr"_sv)
					new_layout->m_keycode_to_key_altgr[*keycode] = *key;
				else
				{
					dprintln("Unrecognized modifier '{}'", pair.front());
					return BAN::Error::from_errno(EINVAL);
				}
			}
		}

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

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wstack-usage="
	static BAN::ErrorOr<void> initialize_name_to_key()
	{
		ASSERT(s_name_to_key.empty());
		TRY(s_name_to_key.insert("A_Ring"_sv,				Key::A_Ring));
		TRY(s_name_to_key.insert("A_Umlaut"_sv,				Key::A_Umlaut));
		TRY(s_name_to_key.insert("A"_sv,						Key::A));
		TRY(s_name_to_key.insert("Acute"_sv,					Key::Acute));
		TRY(s_name_to_key.insert("AltGr"_sv,					Key::AltGr));
		TRY(s_name_to_key.insert("Ampersand"_sv,				Key::Ampersand));
		TRY(s_name_to_key.insert("ArrowDown"_sv,				Key::ArrowDown));
		TRY(s_name_to_key.insert("ArrowLeft"_sv,				Key::ArrowLeft));
		TRY(s_name_to_key.insert("ArrowRight"_sv,			Key::ArrowRight));
		TRY(s_name_to_key.insert("ArrowUp"_sv,				Key::ArrowUp));
		TRY(s_name_to_key.insert("Asterix"_sv,				Key::Asterix));
		TRY(s_name_to_key.insert("AtSign"_sv,				Key::AtSign));
		TRY(s_name_to_key.insert("B"_sv,						Key::B));
		TRY(s_name_to_key.insert("BackSlash"_sv,				Key::BackSlash));
		TRY(s_name_to_key.insert("Backspace"_sv,				Key::Backspace));
		TRY(s_name_to_key.insert("BackTick"_sv,				Key::BackTick));
		TRY(s_name_to_key.insert("BrokenBar"_sv,				Key::BrokenBar));
		TRY(s_name_to_key.insert("C"_sv,						Key::C));
		TRY(s_name_to_key.insert("Calculator"_sv,			Key::Calculator));
		TRY(s_name_to_key.insert("CapsLock"_sv,				Key::CapsLock));
		TRY(s_name_to_key.insert("Caret"_sv,					Key::Caret));
		TRY(s_name_to_key.insert("Cedilla"_sv,				Key::Cedilla));
		TRY(s_name_to_key.insert("CloseCurlyBracket"_sv,		Key::CloseCurlyBracket));
		TRY(s_name_to_key.insert("CloseParenthesis"_sv,		Key::CloseParenthesis));
		TRY(s_name_to_key.insert("CloseSquareBracket"_sv,	Key::CloseSquareBracket));
		TRY(s_name_to_key.insert("Colon"_sv,					Key::Colon));
		TRY(s_name_to_key.insert("Comma"_sv,					Key::Comma));
		TRY(s_name_to_key.insert("Currency"_sv,				Key::Currency));
		TRY(s_name_to_key.insert("D"_sv,						Key::D));
		TRY(s_name_to_key.insert("Delete"_sv,				Key::Delete));
		TRY(s_name_to_key.insert("Dollar"_sv,				Key::Dollar));
		TRY(s_name_to_key.insert("DoubleQuote"_sv,			Key::DoubleQuote));
		TRY(s_name_to_key.insert("E"_sv,						Key::E));
		TRY(s_name_to_key.insert("End"_sv,					Key::End));
		TRY(s_name_to_key.insert("Enter"_sv,					Key::Enter));
		TRY(s_name_to_key.insert("Equals"_sv,				Key::Equals));
		TRY(s_name_to_key.insert("Escape"_sv,				Key::Escape));
		TRY(s_name_to_key.insert("Euro"_sv,					Key::Euro));
		TRY(s_name_to_key.insert("Exclamation"_sv,			Key::ExclamationMark));
		TRY(s_name_to_key.insert("ExclamationMark"_sv,		Key::ExclamationMark));
		TRY(s_name_to_key.insert("F"_sv,						Key::F));
		TRY(s_name_to_key.insert("F1"_sv,					Key::F1));
		TRY(s_name_to_key.insert("F10"_sv,					Key::F10));
		TRY(s_name_to_key.insert("F11"_sv,					Key::F11));
		TRY(s_name_to_key.insert("F12"_sv,					Key::F12));
		TRY(s_name_to_key.insert("F2"_sv,					Key::F2));
		TRY(s_name_to_key.insert("F3"_sv,					Key::F3));
		TRY(s_name_to_key.insert("F4"_sv,					Key::F4));
		TRY(s_name_to_key.insert("F5"_sv,					Key::F5));
		TRY(s_name_to_key.insert("F6"_sv,					Key::F6));
		TRY(s_name_to_key.insert("F7"_sv,					Key::F7));
		TRY(s_name_to_key.insert("F8"_sv,					Key::F8));
		TRY(s_name_to_key.insert("F9"_sv,					Key::F9));
		TRY(s_name_to_key.insert("G"_sv,						Key::G));
		TRY(s_name_to_key.insert("GreaterThan"_sv,			Key::GreaterThan));
		TRY(s_name_to_key.insert("H"_sv,						Key::H));
		TRY(s_name_to_key.insert("Half"_sv,					Key::Half));
		TRY(s_name_to_key.insert("Hashtag"_sv,				Key::Hashtag));
		TRY(s_name_to_key.insert("Home"_sv,					Key::Home));
		TRY(s_name_to_key.insert("Hyphen"_sv,				Key::Hyphen));
		TRY(s_name_to_key.insert("I"_sv,						Key::I));
		TRY(s_name_to_key.insert("Insert"_sv,				Key::Insert));
		TRY(s_name_to_key.insert("J"_sv,						Key::J));
		TRY(s_name_to_key.insert("K"_sv,						Key::K));
		TRY(s_name_to_key.insert("Key0"_sv,					Key::_0));
		TRY(s_name_to_key.insert("Key1"_sv,					Key::_1));
		TRY(s_name_to_key.insert("Key2"_sv,					Key::_2));
		TRY(s_name_to_key.insert("Key3"_sv,					Key::_3));
		TRY(s_name_to_key.insert("Key4"_sv,					Key::_4));
		TRY(s_name_to_key.insert("Key5"_sv,					Key::_5));
		TRY(s_name_to_key.insert("Key6"_sv,					Key::_6));
		TRY(s_name_to_key.insert("Key7"_sv,					Key::_7));
		TRY(s_name_to_key.insert("Key8"_sv,					Key::_8));
		TRY(s_name_to_key.insert("Key9"_sv,					Key::_9));
		TRY(s_name_to_key.insert("L"_sv,						Key::L));
		TRY(s_name_to_key.insert("LAlt"_sv,					Key::LeftAlt));
		TRY(s_name_to_key.insert("LControl"_sv,				Key::LeftCtrl));
		TRY(s_name_to_key.insert("LeftAlt"_sv,				Key::LeftAlt));
		TRY(s_name_to_key.insert("LeftControl"_sv,			Key::LeftCtrl));
		TRY(s_name_to_key.insert("LeftShift"_sv,				Key::LeftShift));
		TRY(s_name_to_key.insert("LessThan"_sv,				Key::LessThan));
		TRY(s_name_to_key.insert("LShift"_sv,				Key::LeftShift));
		TRY(s_name_to_key.insert("M"_sv,						Key::M));
		TRY(s_name_to_key.insert("MediaNext"_sv,				Key::MediaNext));
		TRY(s_name_to_key.insert("MediaPlayPause"_sv,		Key::MediaPlayPause));
		TRY(s_name_to_key.insert("MediaPrevious"_sv,			Key::MediaPrevious));
		TRY(s_name_to_key.insert("MediaStop"_sv,				Key::MediaStop));
		TRY(s_name_to_key.insert("N"_sv,						Key::N));
		TRY(s_name_to_key.insert("Negation"_sv,				Key::Negation));
		TRY(s_name_to_key.insert("None"_sv,					Key::None));
		TRY(s_name_to_key.insert("NumLock"_sv,				Key::NumLock));
		TRY(s_name_to_key.insert("Numpad0"_sv,				Key::Numpad0));
		TRY(s_name_to_key.insert("Numpad1"_sv,				Key::Numpad1));
		TRY(s_name_to_key.insert("Numpad2"_sv,				Key::Numpad2));
		TRY(s_name_to_key.insert("Numpad3"_sv,				Key::Numpad3));
		TRY(s_name_to_key.insert("Numpad4"_sv,				Key::Numpad4));
		TRY(s_name_to_key.insert("Numpad5"_sv,				Key::Numpad5));
		TRY(s_name_to_key.insert("Numpad6"_sv,				Key::Numpad6));
		TRY(s_name_to_key.insert("Numpad7"_sv,				Key::Numpad7));
		TRY(s_name_to_key.insert("Numpad8"_sv,				Key::Numpad8));
		TRY(s_name_to_key.insert("Numpad9"_sv,				Key::Numpad9));
		TRY(s_name_to_key.insert("NumpadDecimal"_sv,			Key::NumpadDecimal));
		TRY(s_name_to_key.insert("NumpadDivide"_sv,			Key::NumpadDivide));
		TRY(s_name_to_key.insert("NumpadEnter"_sv,			Key::NumpadEnter));
		TRY(s_name_to_key.insert("NumpadMinus"_sv,			Key::NumpadMinus));
		TRY(s_name_to_key.insert("NumpadMultiply"_sv,		Key::NumpadMultiply));
		TRY(s_name_to_key.insert("NumpadPlus"_sv,			Key::NumpadPlus));
		TRY(s_name_to_key.insert("O_Umlaut"_sv,				Key::O_Umlaut));
		TRY(s_name_to_key.insert("O"_sv,						Key::O));
		TRY(s_name_to_key.insert("OpenCurlyBracket"_sv,		Key::OpenCurlyBracket));
		TRY(s_name_to_key.insert("OpenParenthesis"_sv,		Key::OpenParenthesis));
		TRY(s_name_to_key.insert("OpenSquareBracket"_sv,		Key::OpenSquareBracket));
		TRY(s_name_to_key.insert("P"_sv,						Key::P));
		TRY(s_name_to_key.insert("PageDown"_sv,				Key::PageDown));
		TRY(s_name_to_key.insert("PageUp"_sv,				Key::PageUp));
		TRY(s_name_to_key.insert("Percent"_sv,				Key::Percent));
		TRY(s_name_to_key.insert("Period"_sv,				Key::Period));
		TRY(s_name_to_key.insert("Pipe"_sv,					Key::Pipe));
		TRY(s_name_to_key.insert("Plus"_sv,					Key::Plus));
		TRY(s_name_to_key.insert("Pound"_sv,					Key::Pound));
		TRY(s_name_to_key.insert("PrintScreen"_sv,			Key::PrintScreen));
		TRY(s_name_to_key.insert("Q"_sv,						Key::Q));
		TRY(s_name_to_key.insert("Question"_sv,				Key::QuestionMark));
		TRY(s_name_to_key.insert("QuestionMark"_sv,			Key::QuestionMark));
		TRY(s_name_to_key.insert("R"_sv,						Key::R));
		TRY(s_name_to_key.insert("RAlt"_sv,					Key::RightAlt));
		TRY(s_name_to_key.insert("RControl"_sv,				Key::RightCtrl));
		TRY(s_name_to_key.insert("RightAlt"_sv,				Key::RightAlt));
		TRY(s_name_to_key.insert("RightControl"_sv,			Key::RightCtrl));
		TRY(s_name_to_key.insert("RightShift"_sv,			Key::RightShift));
		TRY(s_name_to_key.insert("RShift"_sv,				Key::RightShift));
		TRY(s_name_to_key.insert("S"_sv,						Key::S));
		TRY(s_name_to_key.insert("ScrollLock"_sv,			Key::ScrollLock));
		TRY(s_name_to_key.insert("Section"_sv,				Key::Section));
		TRY(s_name_to_key.insert("Semicolon"_sv,				Key::Semicolon));
		TRY(s_name_to_key.insert("SingleQuote"_sv,			Key::SingleQuote));
		TRY(s_name_to_key.insert("Slash"_sv,					Key::Slash));
		TRY(s_name_to_key.insert("Space"_sv,					Key::Space));
		TRY(s_name_to_key.insert("Super"_sv,					Key::Super));
		TRY(s_name_to_key.insert("T"_sv,						Key::T));
		TRY(s_name_to_key.insert("Tab"_sv,					Key::Tab));
		TRY(s_name_to_key.insert("Tilde"_sv,					Key::Tilde));
		TRY(s_name_to_key.insert("TwoDots"_sv,				Key::TwoDots));
		TRY(s_name_to_key.insert("U"_sv,						Key::U));
		TRY(s_name_to_key.insert("Underscore"_sv,			Key::Underscore));
		TRY(s_name_to_key.insert("V"_sv,						Key::V));
		TRY(s_name_to_key.insert("VolumeDown"_sv,			Key::VolumeDown));
		TRY(s_name_to_key.insert("VolumeMute"_sv,			Key::VolumeMute));
		TRY(s_name_to_key.insert("VolumeUp"_sv,				Key::VolumeUp));
		TRY(s_name_to_key.insert("W"_sv,						Key::W));
		TRY(s_name_to_key.insert("X"_sv,						Key::X));
		TRY(s_name_to_key.insert("Y"_sv,						Key::Y));
		TRY(s_name_to_key.insert("Z"_sv,						Key::Z));
		return {};
	}
#pragma GCC diagnostic pop

}
