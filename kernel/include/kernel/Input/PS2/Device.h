#pragma once

#include <kernel/Input/PS2/Controller.h>
#include <kernel/InterruptController.h>

namespace Kernel::Input
{

	class PS2Device : public CharacterDevice, public Interruptable
	{
	public:
		PS2Device(PS2Controller&);
		virtual ~PS2Device() {}

		virtual void send_initialize() = 0;

		virtual void command_timedout(uint8_t* command_data, uint8_t command_size) = 0;

		bool append_command_queue(uint8_t command, uint8_t response_size);
		bool append_command_queue(uint8_t command, uint8_t data, uint8_t response_size);
		virtual void handle_irq() final override;

		virtual void handle_byte(uint8_t) = 0;

		virtual BAN::StringView name() const final override { return m_name; }
		virtual dev_t rdev() const final override { return m_rdev; }

		virtual void update() final override { m_controller.update_command_queue(); }

	private:
		const BAN::String m_name;
		const dev_t m_rdev;
		PS2Controller& m_controller;
	};

}
