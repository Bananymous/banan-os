#pragma once

#include <BAN/CircularQueue.h>
#include <kernel/Device/Device.h>
#include <kernel/InterruptController.h>

namespace Kernel::Input
{

	class PS2Device;

	class PS2Controller
	{
	public:
		static BAN::ErrorOr<void> initialize();
		static PS2Controller& get();

		void send_byte(const PS2Device*, uint8_t);

	private:
		PS2Controller() = default;
		BAN::ErrorOr<void> initialize_impl();
		BAN::ErrorOr<void> initialize_device(uint8_t);

		BAN::ErrorOr<void> reset_device(uint8_t);
		BAN::ErrorOr<void> set_scanning(uint8_t, bool);

	private:
		PS2Device* m_devices[2] { nullptr, nullptr };
	};

}
