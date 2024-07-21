#pragma once

#include <BAN/ByteSpan.h>

#include <kernel/Device/Device.h>
#include <kernel/ThreadBlocker.h>

namespace Kernel
{

	class InputDevice : public CharacterDevice, public BAN::Weakable<InputDevice>
	{
	public:
		enum class Type
		{
			Mouse,
			Keyboard,
		};

	public:
		InputDevice(Type type);

		BAN::StringView name() const final override { return m_name; }
		dev_t rdev() const final override { return m_rdev; }

	protected:
		void add_event(BAN::ConstByteSpan);

		BAN::ErrorOr<size_t> read_impl(off_t, BAN::ByteSpan) override;

		bool can_read_impl() const override { SpinLockGuard _(m_event_lock); return m_event_count > 0; }
		bool can_write_impl() const override { return false; }
		bool has_error_impl() const override { return false; }


	private:
		BAN::ErrorOr<size_t> read_non_block(BAN::ByteSpan);

	private:
		const dev_t m_rdev;
		const BAN::String m_name;

		const Type m_type;

		mutable SpinLock m_event_lock;
		ThreadBlocker m_event_thread_blocker;

		static constexpr size_t m_max_event_count { 128 };

		BAN::Vector<uint8_t> m_event_buffer;
		const size_t m_event_size;
		size_t m_event_tail { 0 };
		size_t m_event_head { 0 };
		size_t m_event_count { 0 };

		friend class KeyboardDevice;
		friend class MouseDevice;
	};



	class KeyboardDevice : public CharacterDevice
	{
	public:
		static BAN::ErrorOr<BAN::RefPtr<KeyboardDevice>> create(mode_t mode, uid_t uid, gid_t gid);

		void notify() { m_thread_blocker.unblock(); }

	private:
		KeyboardDevice(mode_t mode, uid_t uid, gid_t gid);
		BAN::ErrorOr<size_t> read_impl(off_t, BAN::ByteSpan) override;

		bool can_read_impl() const override;
		bool can_write_impl() const override { return false; }
		bool has_error_impl() const override { return false; }

		BAN::StringView name() const final override { return m_name; }
		dev_t rdev() const final override { return m_rdev; }

	private:
		const dev_t m_rdev;
		const BAN::StringView m_name;
		ThreadBlocker m_thread_blocker;

		friend class BAN::RefPtr<KeyboardDevice>;
	};

	class MouseDevice : public CharacterDevice
	{
	public:
		static BAN::ErrorOr<BAN::RefPtr<MouseDevice>> create(mode_t mode, uid_t uid, gid_t gid);

		void notify() { m_thread_blocker.unblock(); }

	private:
		MouseDevice(mode_t mode, uid_t uid, gid_t gid);
		BAN::ErrorOr<size_t> read_impl(off_t, BAN::ByteSpan) override;

		bool can_read_impl() const override;
		bool can_write_impl() const override { return false; }
		bool has_error_impl() const override { return false; }

		BAN::StringView name() const final override { return m_name; }
		dev_t rdev() const final override { return m_rdev; }

	private:
		const dev_t m_rdev;
		const BAN::StringView m_name;
		ThreadBlocker m_thread_blocker;

		friend class BAN::RefPtr<MouseDevice>;
	};

}
