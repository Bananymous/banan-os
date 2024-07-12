#pragma once

#include <kernel/Input/PS2/Controller.h>
#include <kernel/Input/InputDevice.h>
#include <kernel/Interruptable.h>

namespace Kernel::Input
{

	class PS2Device : public Interruptable, public InputDevice
	{
	public:
		virtual void send_initialize() = 0;

		virtual void command_timedout(uint8_t* command_data, uint8_t command_size) = 0;

		bool append_command_queue(uint8_t command, uint8_t response_size);
		bool append_command_queue(uint8_t command, uint8_t data, uint8_t response_size);
		virtual void handle_irq() final override;

		virtual void handle_byte(uint8_t) = 0;

	protected:
		PS2Device(PS2Controller&, InputDevice::Type type);

	protected:
		PS2Controller& m_controller;
	};

}
