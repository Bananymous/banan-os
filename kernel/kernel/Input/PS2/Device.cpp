#include <kernel/FS/DevFS/FileSystem.h>
#include <kernel/Input/PS2/Config.h>
#include <kernel/Input/PS2/Device.h>
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

	bool PS2Device::append_command_queue(uint8_t command, uint8_t response_size)
	{
		CriticalScope _;
		if (m_command_queue.size() + 1 >= m_command_queue.capacity())
		{
			dprintln("PS/2 command queue full");
			return false;
		}
		m_command_queue.push(Command {
			.out_data	= { command, 0x00 },
			.out_count	= 1,
			.in_count	= response_size,
			.send_index	= 0
		});
		update_command();
		return true;
	}

	bool PS2Device::append_command_queue(uint8_t command, uint8_t data, uint8_t response_size)
	{
		CriticalScope _;
		if (m_command_queue.size() + 1 >= m_command_queue.capacity())
		{
			dprintln("PS/2 command queue full");
			return false;
		}
		m_command_queue.push(Command {
			.out_data	= { command, data },
			.out_count	= 2,
			.in_count	= response_size,
			.send_index	= 0
		});
		update_command();
		return true;
	}

	void PS2Device::handle_irq()
	{
		uint8_t byte = IO::inb(PS2::IOPort::DATA);

		switch (m_state)
		{
			case State::WaitingAck:
			{
				switch (byte)
				{
					case PS2::Response::ACK:
					{
						auto& command = m_command_queue.front();
						if (++command.send_index < command.out_count)
							m_state = State::Normal;
						else if (command.in_count > 0)
							m_state = State::WaitingResponse;
						else
						{
							m_command_queue.pop();
							m_state = State::Normal;
							m_controller.unlock_command(this);
						}
						break;
					}
					case PS2::Response::RESEND:
						m_state = State::Normal;
						break;
					default:
						handle_device_command_response(byte);
						break;
				}
				break;
			}
			case State::WaitingResponse:
			{
				if (--m_command_queue.front().in_count <= 0)
				{
					m_command_queue.pop();
					m_state = State::Normal;
					m_controller.unlock_command(this);
				}
				handle_byte(byte);
				break;
			}
			case State::Normal:
			{
				handle_byte(byte);
				break;
			}
		}

		update_command();
	}

	void PS2Device::update_command()
	{
		ASSERT(!interrupts_enabled());

		if (m_state != State::Normal)
			return;
		if (m_command_queue.empty())
			return;

		const auto& command = m_command_queue.front();
		ASSERT(command.send_index < command.out_count);

		if (!m_controller.lock_command(this))
			return;

		m_state = State::WaitingAck;
		auto ret = m_controller.device_send_byte(this, command.out_data[command.send_index]);
		if (ret.is_error())
			dwarnln("Could not send byte to device: {}", ret.error());
	}

}
