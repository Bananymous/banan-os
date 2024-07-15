#include <kernel/Device/DeviceNumbers.h>
#include <kernel/FS/DevFS/FileSystem.h>
#include <kernel/Input/InputDevice.h>
#include <kernel/Lock/LockGuard.h>

#include <LibInput/KeyEvent.h>
#include <LibInput/MouseEvent.h>

#include <sys/sysmacros.h>

namespace Kernel
{

	static BAN::Vector<BAN::WeakPtr<InputDevice>> s_keyboards;
	static BAN::RefPtr<KeyboardDevice> s_keyboard_device;

	static BAN::Vector<BAN::WeakPtr<InputDevice>> s_mice;
	static BAN::RefPtr<MouseDevice> s_mouse_device;

	static const char* get_name_format(InputDevice::Type type)
	{
		switch (type)
		{
			case InputDevice::Type::Keyboard:
				return "keyboard{}";
			case InputDevice::Type::Mouse:
				return "mouse{}";
		}
		ASSERT_NOT_REACHED();
	}

	static dev_t get_rdev(InputDevice::Type type)
	{
		switch (type)
		{
			case InputDevice::Type::Keyboard:
				for (size_t i = 0; i < s_keyboards.size(); i++)
					if (!s_keyboards[i].valid())
						return makedev(DeviceNumber::Keyboard, i + 1);
				return makedev(DeviceNumber::Keyboard, s_keyboards.size() + 1);
			case InputDevice::Type::Mouse:
				for (size_t i = 0; i < s_mice.size(); i++)
					if (!s_mice[i].valid())
						return makedev(DeviceNumber::Mouse, i + 1);
				return makedev(DeviceNumber::Mouse, s_mice.size() + 1);
		}
		ASSERT_NOT_REACHED();
	}

	static size_t get_event_size(InputDevice::Type type)
	{
		switch (type)
		{
			case InputDevice::Type::Keyboard:
				return sizeof(LibInput::RawKeyEvent);
			case InputDevice::Type::Mouse:
				return sizeof(LibInput::MouseEvent);
		}
		ASSERT_NOT_REACHED();
	}

	InputDevice::InputDevice(Type type)
		: CharacterDevice(0440, 0, 901)
		, m_rdev(get_rdev(type))
		, m_name(MUST(BAN::String::formatted(get_name_format(type), minor(m_rdev) - 1)))
		, m_type(type)
		, m_event_size(get_event_size(type))
	{
		MUST(m_event_buffer.resize(m_event_size * m_max_event_count, 0));

		if (m_type == Type::Keyboard)
		{
			if (s_keyboards.size() < minor(m_rdev))
				MUST(s_keyboards.resize(minor(m_rdev)));
			s_keyboards[minor(m_rdev) - 1] = MUST(get_weak_ptr());
		}

		if (m_type == Type::Mouse)
		{
			if (s_mice.size() < minor(m_rdev))
				MUST(s_mice.resize(minor(m_rdev)));
			s_mice[minor(m_rdev) - 1] = MUST(get_weak_ptr());
		}
	}

	void InputDevice::add_event(BAN::ConstByteSpan event)
	{
		SpinLockGuard _(m_event_lock);
		ASSERT(event.size() == m_event_size);

		if (m_type == Type::Mouse && m_event_count > 0)
		{
			const size_t last_index = (m_event_head + m_max_event_count - 1) % m_max_event_count;

			auto& last_event = *reinterpret_cast<LibInput::MouseEvent*>(&m_event_buffer[last_index * m_event_size]);
			auto& curr_event = event.as<const LibInput::MouseEvent>();
			if (last_event.type == LibInput::MouseEventType::MouseMoveEvent && curr_event.type == LibInput::MouseEventType::MouseMoveEvent)
			{
				last_event.move_event.rel_x += curr_event.move_event.rel_x;
				last_event.move_event.rel_y += curr_event.move_event.rel_y;
				return;
			}
			if (last_event.type == LibInput::MouseEventType::MouseScrollEvent && curr_event.type == LibInput::MouseEventType::MouseScrollEvent)
			{
				last_event.scroll_event.scroll += curr_event.scroll_event.scroll;
				return;
			}
		}

		if (m_event_count == m_max_event_count)
		{
			m_event_tail = (m_event_tail + 1) % m_max_event_count;
			m_event_count--;
		}

		memcpy(&m_event_buffer[m_event_head * m_event_size], event.data(), m_event_size);
		m_event_head = (m_event_head + 1) % m_max_event_count;
		m_event_count++;

		m_event_semaphore.unblock();
		if (m_type == Type::Keyboard && s_keyboard_device)
			s_keyboard_device->notify();
		if (m_type == Type::Mouse && s_mouse_device)
			s_mouse_device->notify();
	}

	BAN::ErrorOr<size_t> InputDevice::read_impl(off_t, BAN::ByteSpan buffer)
	{
		if (buffer.size() < m_event_size)
			return BAN::Error::from_errno(ENOBUFS);

		auto state = m_event_lock.lock();
		while (m_event_count == 0)
		{
			m_event_lock.unlock(state);
			{
				LockFreeGuard _(m_mutex);
				TRY(Thread::current().block_or_eintr_indefinite(m_event_semaphore));
			}
			state = m_event_lock.lock();
		}

		memcpy(buffer.data(), &m_event_buffer[m_event_tail * m_event_size], m_event_size);
		m_event_tail = (m_event_tail + 1) % m_max_event_count;
		m_event_count--;

		m_event_lock.unlock(state);

		return m_event_size;
	}

	BAN::ErrorOr<size_t> InputDevice::read_non_block(BAN::ByteSpan buffer)
	{
		if (buffer.size() < m_event_size)
			return BAN::Error::from_errno(ENOBUFS);

		SpinLockGuard _(m_event_lock);

		if (m_event_count == 0)
			return 0;

		memcpy(buffer.data(), &m_event_buffer[m_event_tail * m_event_size], m_event_size);
		m_event_tail = (m_event_tail + 1) % m_max_event_count;
		m_event_count--;

		return m_event_size;
	}



	BAN::ErrorOr<BAN::RefPtr<KeyboardDevice>> KeyboardDevice::create(mode_t mode, uid_t uid, gid_t gid)
	{
		s_keyboard_device = TRY(BAN::RefPtr<KeyboardDevice>::create(mode, uid, gid));
		return s_keyboard_device;
	}

	KeyboardDevice::KeyboardDevice(mode_t mode, uid_t uid, gid_t gid)
		: CharacterDevice(mode, uid, gid)
		, m_rdev(makedev(DeviceNumber::Keyboard, 0))
		, m_name("keyboard"_sv)
	{}

	BAN::ErrorOr<size_t> KeyboardDevice::read_impl(off_t, BAN::ByteSpan buffer)
	{
		if (buffer.size() < sizeof(LibInput::RawKeyEvent))
			return BAN::Error::from_errno(ENOBUFS);

		for (;;)
		{
			for (auto& weak_keyboard : s_keyboards)
			{
				auto keyboard = weak_keyboard.lock();
				if (!keyboard)
					continue;

				auto bytes = TRY(keyboard->read_non_block(buffer));
				if (bytes > 0)
					return bytes;
			}

			LockFreeGuard _(m_mutex);
			TRY(Thread::current().block_or_eintr_indefinite(m_semaphore));
		}
	}

	bool KeyboardDevice::can_read_impl() const
	{
		for (auto& weak_keyboard : s_keyboards)
			if (auto keyboard = weak_keyboard.lock())
				if (keyboard->can_read())
					return true;
		return false;
	}



	BAN::ErrorOr<BAN::RefPtr<MouseDevice>> MouseDevice::create(mode_t mode, uid_t uid, gid_t gid)
	{
		s_mouse_device = TRY(BAN::RefPtr<MouseDevice>::create(mode, uid, gid));
		return s_mouse_device;
	}

	MouseDevice::MouseDevice(mode_t mode, uid_t uid, gid_t gid)
		: CharacterDevice(mode, uid, gid)
		, m_rdev(makedev(DeviceNumber::Mouse, 0))
		, m_name("mouse"_sv)
	{}

	BAN::ErrorOr<size_t> MouseDevice::read_impl(off_t, BAN::ByteSpan buffer)
	{
		if (buffer.size() < sizeof(LibInput::MouseEvent))
			return BAN::Error::from_errno(ENOBUFS);

		for (;;)
		{
			for (auto& weak_mouse : s_mice)
			{
				auto mouse = weak_mouse.lock();
				if (!mouse)
					continue;

				auto bytes = TRY(mouse->read_non_block(buffer));
				if (bytes > 0)
					return bytes;
			}

			LockFreeGuard _(m_mutex);
			TRY(Thread::current().block_or_eintr_indefinite(m_semaphore));
		}
	}

	bool MouseDevice::can_read_impl() const
	{
		for (auto& weak_mouse : s_mice)
			if (auto mouse = weak_mouse.lock())
				if (mouse->can_read())
					return true;
		return false;
	}

}
