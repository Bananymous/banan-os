#pragma once

#include <BAN/Array.h>
#include <BAN/StringView.h>
#include <BAN/UniqPtr.h>
#include <LibInput/KeyEvent.h>

namespace LibInput
{

	class KeyboardLayout
	{
	public:
		static BAN::ErrorOr<void> initialize();
		static KeyboardLayout& get();

		KeyEvent key_event_from_raw(RawKeyEvent);
		BAN::ErrorOr<void> load_from_file(BAN::StringView path);

	private:
		KeyboardLayout();

	private:
		BAN::Array<Key, 0xFF> m_keycode_to_key_normal;
		BAN::Array<Key, 0xFF> m_keycode_to_key_shift;
		BAN::Array<Key, 0xFF> m_keycode_to_key_altgr;
		friend class BAN::UniqPtr<KeyboardLayout>;
	};

}
