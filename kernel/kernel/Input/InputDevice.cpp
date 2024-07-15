#include <kernel/Device/DeviceNumbers.h>
#include <kernel/Input/InputDevice.h>
#include <kernel/Lock/LockGuard.h>

#include <LibInput/KeyEvent.h>
#include <LibInput/MouseEvent.h>

#include <sys/sysmacros.h>

namespace Kernel
{

	static BAN::Atomic<uint16_t> s_next_keyboard { 0 };
	static BAN::Atomic<uint16_t> s_next_mouse { 0 };

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
				return makedev(DeviceNumber::Keyboard, s_next_keyboard++);
			case InputDevice::Type::Mouse:
				return makedev(DeviceNumber::Mouse, s_next_mouse++);
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
		, m_name(MUST(BAN::String::formatted(get_name_format(type), minor(m_rdev))))
		, m_type(type)
		, m_event_size(get_event_size(type))
	{
		MUST(m_event_buffer.resize(m_event_size * m_max_event_count, 0));
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

}
