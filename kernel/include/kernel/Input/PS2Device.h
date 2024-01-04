#include <kernel/Input/PS2Controller.h>

namespace Kernel::Input
{

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

		virtual BAN::StringView name() const final override { return m_name; }
		virtual dev_t rdev() const final override { return m_rdev; }

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
		const dev_t m_rdev;

		PS2Controller&					m_controller;
		State							m_state			= State::Normal;
		BAN::CircularQueue<uint8_t, 10>	m_command_queue;
	};

}