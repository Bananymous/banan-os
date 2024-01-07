#pragma once

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

		BAN::ErrorOr<void> device_send_byte(PS2Device* device, uint8_t byte);

		[[nodiscard]] bool lock_command(PS2Device*);
		void unlock_command(PS2Device*);

	private:
		PS2Controller() = default;
		BAN::ErrorOr<void> initialize_impl();
		BAN::ErrorOr<void> initialize_device(uint8_t);

		BAN::ErrorOr<void> reset_device(uint8_t);
		BAN::ErrorOr<void> set_scanning(uint8_t, bool);

		BAN::ErrorOr<uint8_t> read_byte();
		BAN::ErrorOr<void> send_byte(uint16_t port, uint8_t byte);

		BAN::ErrorOr<void> send_command(PS2::Command command);
		BAN::ErrorOr<void> send_command(PS2::Command command, uint8_t data);

		BAN::ErrorOr<void> device_send_byte(uint8_t device_index, uint8_t byte);
		BAN::ErrorOr<void> device_read_ack(uint8_t device_index);

	private:
		BAN::RefPtr<PS2Device> m_devices[2];
		RecursiveSpinLock m_lock;

		PS2Device* m_executing_device	{ nullptr };
		PS2Device* m_pending_device		{ nullptr };
	};

}
