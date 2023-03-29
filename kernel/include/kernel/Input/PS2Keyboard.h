#pragma once

#include <BAN/CircularQueue.h>
#include <kernel/Input/KeyEvent.h>
#include <kernel/Input/PS2Controller.h>
#include <kernel/Input/PS2Keymap.h>

namespace Kernel::Input
{

	class PS2Keyboard final : public PS2Device
	{
	private:
		enum Command : uint8_t
		{
			SET_LEDS = 0xED,
			SCANCODE = 0xF0,
			ENABLE_SCANNING = 0xF4,
			DISABLE_SCANNING = 0xF5,
		};

		enum class State
		{
			Normal,
			WaitingAck,
		};

	public:
		static BAN::ErrorOr<PS2Keyboard*> create(PS2Controller&);

		virtual void on_byte(uint8_t) override;
		virtual void update() override;

	private:
		PS2Keyboard(PS2Controller& controller)
			: m_controller(controller)
		{}
		BAN::ErrorOr<void> initialize();

		void append_command_queue(uint8_t);
		void append_command_queue(uint8_t, uint8_t);

		void buffer_has_key();

		void update_leds();

	private:
		PS2Controller& m_controller;
		uint8_t m_byte_buffer[10];
		uint8_t m_byte_index { 0 };

		uint8_t m_modifiers { 0 };

		BAN::CircularQueue<KeyEvent, 10> m_event_queue;
		BAN::CircularQueue<uint8_t, 10> m_command_queue;

		PS2Keymap m_keymap;

		State m_state { State::Normal };

	public:
		virtual ino_t ino() const override { return 0; }
		virtual mode_t mode() const override { return IFCHR | IRUSR | IRGRP; }
		virtual nlink_t nlink() const override { return 0; }
		virtual uid_t uid() const override { return 0; }
		virtual gid_t gid() const override { return 0; }
		virtual off_t size() const override { return 0; }
		virtual timespec atime() const override { return { 0, 0 }; }
		virtual timespec mtime() const override { return { 0, 0 }; }
		virtual timespec ctime() const override { return { 0, 0 }; }
		virtual blksize_t blksize() const override { return sizeof(KeyEvent); }
		virtual blkcnt_t blocks() const override { return 0; }

		virtual BAN::StringView name() const override { return "input"sv; }

		virtual BAN::ErrorOr<size_t> read(size_t, void*, size_t) override;
		virtual BAN::ErrorOr<void> create_file(BAN::StringView, mode_t) override { return BAN::Error::from_errno(ENOTDIR); }

		virtual InodeType type() const override { return InodeType::Device; }
		virtual bool operator==(const Inode&) const override { return false; }

	protected:
		virtual BAN::ErrorOr<BAN::RefPtr<Inode>> read_directory_inode_impl(BAN::StringView) override { return BAN::Error::from_errno(ENOTDIR); }
		virtual BAN::ErrorOr<BAN::Vector<BAN::String>> read_directory_entries_impl(size_t) override { return BAN::Error::from_errno(ENOTDIR); }
	};

}