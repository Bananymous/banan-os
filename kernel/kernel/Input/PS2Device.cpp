#include <kernel/FS/DevFS/FileSystem.h>
#include <kernel/Input/PS2Config.h>
#include <kernel/Input/PS2Device.h>
#include <kernel/IO.h>

#include <sys/sysmacros.h>

namespace Kernel::Input
{

	PS2Device::PS2Device(PS2Controller& controller)
		: CharacterDevice(0440, 0, 901)
		, m_name(BAN::String::formatted("input{}", DevFileSystem::get().get_next_input_device()))
		, m_rdev(makedev(DevFileSystem::get().get_next_dev(), 0))
		, m_controller(controller)
	{ }

	bool PS2Device::append_command_queue(uint8_t command)
	{
		if (m_command_queue.size() + 1 >= m_command_queue.capacity())
		{
			dprintln("PS/2 command queue full");
			return false;
		}
		m_command_queue.push(command);
		update();
		return true;
	}

	bool PS2Device::append_command_queue(uint8_t command, uint8_t data)
	{
		if (m_command_queue.size() + 2 >= m_command_queue.capacity())
		{
			dprintln("PS/2 command queue full");
			return false;
		}
		m_command_queue.push(command);
		m_command_queue.push(data);
		update();
		return true;
	}

	void PS2Device::handle_irq()
	{
		uint8_t byte = IO::inb(PS2::IOPort::DATA);

		// NOTE: This implementation does not allow using commands
		//       that respond with more bytes than ACK
		switch (m_state)
		{
			case State::WaitingAck:
			{
				switch (byte)
				{
					case PS2::Response::ACK:
						m_command_queue.pop();
						m_state = State::Normal;
						break;
					case PS2::Response::RESEND:
						m_state = State::Normal;
						break;
					default:
						handle_device_command_response(byte);
						break;
				}
				break;
			}
			case State::Normal:
			{
				handle_byte(byte);
				break;
			}
		}

		update();
	}

	void PS2Device::update()
	{
		if (m_state == State::WaitingAck)
			return;
		if (m_command_queue.empty())
			return;
		m_state = State::WaitingAck;
		m_controller.send_byte(this, m_command_queue.front());
	}

}