#pragma once

#include <BAN/CircularQueue.h>
#include <BAN/UniqPtr.h>
#include <kernel/Device/Device.h>
#include <kernel/Input/PS2/Config.h>
#include <kernel/InterruptController.h>
#include <kernel/SpinLock.h>

namespace Kernel::Input
{

	class PS2Device;

	class PS2Controller
	{
	public:
		static BAN::ErrorOr<void> initialize();
		static PS2Controller& get();

		bool append_command_queue(PS2Device*, uint8_t command, uint8_t response_size);
		bool append_command_queue(PS2Device*, uint8_t command, uint8_t data, uint8_t response_size);
		void update_command_queue();
		// Returns true, if byte is used as command, if returns false, byte is meant to device
		bool handle_command_byte(PS2Device*, uint8_t);

	private:
		PS2Controller() = default;
		BAN::ErrorOr<void> initialize_impl();
		BAN::ErrorOr<void> initialize_device(uint8_t);

		BAN::ErrorOr<uint8_t> read_byte();
		BAN::ErrorOr<void> send_byte(uint16_t port, uint8_t byte);

		BAN::ErrorOr<void> send_command(PS2::Command command);
		BAN::ErrorOr<void> send_command(PS2::Command command, uint8_t data);

		BAN::ErrorOr<void> device_send_byte(uint8_t device_index, uint8_t byte);
		BAN::ErrorOr<void> device_send_byte_and_wait_ack(uint8_t device_index, uint8_t byte);

	private:
		struct Command
		{
			enum class State : uint8_t
			{
				NotSent,
				Sending,
				WaitingAck,
				WaitingResponse,
			};
			State   state;
			uint8_t device_index;
			uint8_t out_data[2];
			uint8_t out_count;
			uint8_t in_count;
			uint8_t send_index;
		};

	private:
		BAN::RefPtr<PS2Device> m_devices[2];
		RecursiveSpinLock m_lock;

		BAN::CircularQueue<Command, 128> m_command_queue;
	};

}
