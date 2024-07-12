#pragma once

#include <kernel/Input/PS2/Device.h>

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
		static BAN::ErrorOr<BAN::RefPtr<PS2Mouse>> create(PS2Controller&);
		virtual void send_initialize() override;

		virtual void command_timedout(uint8_t* command_data, uint8_t command_size) final override { (void)command_data; (void)command_size; }

		virtual void handle_byte(uint8_t) final override;

		virtual void update() final override { m_controller.update_command_queue(); }

	private:
		PS2Mouse(PS2Controller& controller);

		void initialize_extensions(uint8_t);

	private:
		uint8_t m_byte_buffer[10];
		uint8_t m_byte_index { 0 };

		bool	m_enabled		{ false };
		uint8_t m_mouse_id		{ 0x00 };
		uint8_t m_button_mask	{ 0x00 };

		friend class BAN::RefPtr<PS2Mouse>;
	};

}
