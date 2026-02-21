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

		BAN::Span<const Key> keymap_normal() const { return m_keycode_to_key_normal.span(); }
		BAN::Span<const Key> keymap_shift() const  { return m_keycode_to_key_shift.span(); }
		BAN::Span<const Key> keymap_altgr() const  { return m_keycode_to_key_altgr.span(); }

	private:
		KeyboardLayout();

	private:
		BAN::Array<Key, 0x100> m_keycode_to_key_normal;
		BAN::Array<Key, 0x100> m_keycode_to_key_shift;
		BAN::Array<Key, 0x100> m_keycode_to_key_altgr;
		friend class BAN::UniqPtr<KeyboardLayout>;
	};

}
