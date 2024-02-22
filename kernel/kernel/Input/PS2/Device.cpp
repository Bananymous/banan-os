#include <kernel/Device/DeviceNumbers.h>
#include <kernel/FS/DevFS/FileSystem.h>
#include <kernel/Input/PS2/Config.h>
#include <kernel/Input/PS2/Device.h>
#include <kernel/IO.h>

#include <sys/sysmacros.h>

namespace Kernel::Input
{

	PS2Device::PS2Device(PS2Controller& controller)
		: CharacterDevice(0440, 0, 901)
		, m_rdev(makedev(DeviceNumber::Input, DevFileSystem::get().get_next_input_device()))
		, m_name(BAN::String::formatted("input{}", minor(m_rdev)))
		, m_controller(controller)
	{ }

	bool PS2Device::append_command_queue(uint8_t command, uint8_t response_size)
	{
		return m_controller.append_command_queue(this, command, response_size);
	}

	bool PS2Device::append_command_queue(uint8_t command, uint8_t data, uint8_t response_size)
	{
		return m_controller.append_command_queue(this, command, data, response_size);
	}

	void PS2Device::handle_irq()
	{
		uint8_t byte = IO::inb(PS2::IOPort::DATA);
		if (!m_controller.handle_command_byte(this, byte))
			handle_byte(byte);
	}

}
