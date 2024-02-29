#pragma once

#include <kernel/Input/MouseEvent.h>
#include <kernel/Input/PS2/Device.h>
#include <kernel/Semaphore.h>

namespace Kernel::Input
{

	class PS2Mouse final : public PS2Device
	{
	private:
		enum Command : uint8_t
		{
			SET_SAMPLE_RATE = 0xF3
		};

	public:
		static BAN::ErrorOr<PS2Mouse*> create(PS2Controller&);
		virtual void send_initialize() override;

		virtual void command_timedout(uint8_t* command_data, uint8_t command_size) final override {}

		virtual void handle_byte(uint8_t) final override;

	private:
		PS2Mouse(PS2Controller& controller);

		void initialize_extensions(uint8_t);

	private:
		uint8_t m_byte_buffer[10];
		uint8_t m_byte_index { 0 };

		bool	m_enabled		{ false };
		uint8_t m_mouse_id		{ 0x00 };
		uint8_t m_button_mask	{ 0x00 };

		BAN::CircularQueue<MouseEvent, 128> m_event_queue;
		SpinLock m_event_lock;

		Semaphore m_semaphore;

	protected:
		virtual BAN::ErrorOr<size_t> read_impl(off_t, BAN::ByteSpan) override;

		virtual bool can_read_impl() const override { return !m_event_queue.empty(); }
		virtual bool can_write_impl() const override { return false; }
		virtual bool has_error_impl() const override { return false; }
	};

}
