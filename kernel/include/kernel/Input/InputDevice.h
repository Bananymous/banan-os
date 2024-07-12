#pragma once

#include <BAN/ByteSpan.h>

#include <kernel/Device/Device.h>

namespace Kernel
{

	class InputDevice : public CharacterDevice
	{
	public:
		enum class Type
		{
			Mouse,
			Keyboard,
		};

	public:
		InputDevice(Type type);

	protected:
		void add_event(BAN::ConstByteSpan);

		BAN::ErrorOr<size_t> read_impl(off_t, BAN::ByteSpan) override;

		bool can_read_impl() const override { SpinLockGuard _(m_event_lock); return m_event_count > 0; }
		bool can_write_impl() const override { return false; }
		bool has_error_impl() const override { return false; }

		virtual BAN::StringView name() const final override { return m_name; }
		virtual dev_t rdev() const final override { return m_rdev; }

	private:
		const dev_t m_rdev;
		const BAN::String m_name;

		mutable SpinLock m_event_lock;
		Semaphore m_event_semaphore;

		static constexpr size_t m_max_event_count { 128 };

		BAN::Vector<uint8_t> m_event_buffer;
		const size_t m_event_size;
		size_t m_event_tail { 0 };
		size_t m_event_head { 0 };
		size_t m_event_count { 0 };
	};

}
