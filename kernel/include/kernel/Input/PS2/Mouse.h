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

		virtual void handle_byte(uint8_t) final override;
		virtual void handle_device_command_response(uint8_t) final override;

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

		Semaphore m_semaphore;

	protected:
		virtual BAN::ErrorOr<size_t> read_impl(off_t, BAN::ByteSpan) override;
		virtual bool has_data_impl() const override;
	};

}
