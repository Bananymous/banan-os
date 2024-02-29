#pragma once

#include <BAN/Array.h>
#include <BAN/UniqPtr.h>
#include <kernel/Input/KeyEvent.h>
#include <kernel/Lock/SpinLock.h>

namespace Kernel::Input
{

	class KeyboardLayout
	{
	public:
		static BAN::ErrorOr<void> initialize();
		static KeyboardLayout& get();

		Key get_key_from_event(KeyEvent);
		BAN::ErrorOr<void> load_from_file(BAN::StringView path);

	private:
		KeyboardLayout();

	private:
		BAN::Array<Key, 0xFF> m_keycode_to_key_normal;
		BAN::Array<Key, 0xFF> m_keycode_to_key_shift;
		BAN::Array<Key, 0xFF> m_keycode_to_key_altgr;
		SpinLock m_lock;

		friend class BAN::UniqPtr<KeyboardLayout>;
	};

}
