#pragma once

#include <BAN/CircularQueue.h>
#include <kernel/Device/Device.h>
#include <kernel/InterruptController.h>

namespace Kernel::Input
{

	class PS2Controller;

	class PS2Device : public CharacterDevice, public Interruptable
	{
	public:
		PS2Device(PS2Controller&);
		virtual ~PS2Device() {}

		virtual void send_initialize() = 0;

		bool append_command_queue(uint8_t command);
		bool append_command_queue(uint8_t command, uint8_t data);
		virtual void handle_irq() final override;

		virtual void handle_byte(uint8_t) = 0;
		virtual void handle_device_command_response(uint8_t) = 0;

		virtual BAN::StringView name() const override { return m_name; }

	protected:
		void update();

	private:
		enum class State
		{
			Normal,
			WaitingAck,
		};

	private:
		const BAN::String m_name;

		PS2Controller&					m_controller;
		State							m_state			= State::Normal;
		BAN::CircularQueue<uint8_t, 10>	m_command_queue;
	};

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
