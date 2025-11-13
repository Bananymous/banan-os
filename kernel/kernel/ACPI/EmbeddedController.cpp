#include <BAN/ScopeGuard.h>

#include <kernel/ACPI/ACPI.h>
#include <kernel/ACPI/EmbeddedController.h>
#include <kernel/IO.h>
#include <kernel/Lock/LockGuard.h>
#include <kernel/Timer/Timer.h>

namespace Kernel::ACPI
{

	enum Status
	{
		STS_OBF     = 1 << 0,
		STS_IBF     = 1 << 1,
		STS_CMD     = 1 << 3,
		STS_BURST   = 1 << 4,
		STS_SCI_EVT = 1 << 5,
		STS_SMI_EVT = 1 << 6,
	};

	enum Command
	{
		CMD_READ      = 0x80,
		CMD_WRITE     = 0x81,
		CMD_BURST_EN  = 0x82,
		CMD_BURST_DIS = 0x83,
		CMD_QUERY     = 0x84,
	};

	BAN::ErrorOr<BAN::UniqPtr<EmbeddedController>> EmbeddedController::create(AML::Scope&& scope, uint16_t command_port, uint16_t data_port, BAN::Optional<uint8_t> gpe)
	{
		auto* embedded_controller_ptr = new EmbeddedController(BAN::move(scope), command_port, data_port, gpe.has_value());
		if (embedded_controller_ptr == nullptr)
			return BAN::Error::from_errno(ENOMEM);

		auto* thread = TRY(Thread::create_kernel([](void* ec) { static_cast<EmbeddedController*>(ec)->thread_task(); }, embedded_controller_ptr));
		TRY(Processor::scheduler().add_thread(thread));

		auto embedded_controller = BAN::UniqPtr<EmbeddedController>::adopt(embedded_controller_ptr);
		embedded_controller->m_thread = thread;

		if (gpe.has_value())
			TRY(ACPI::get().register_gpe_handler(gpe.value(), &handle_gpe_wrapper, embedded_controller.ptr()));
		else
		{
			// FIXME: Restructure EC such that SCI_EVT can be polled.
			//        Simple solution would be spawning another thread,
			//        but that feels too hacky.
			dwarnln("TODO: SCI_EVT polling without GPE");
		}

		return embedded_controller;
	}

	EmbeddedController::~EmbeddedController()
	{
		if (m_thread)
			m_thread->add_signal(SIGKILL, {});
		m_thread = nullptr;
	}

	BAN::ErrorOr<uint8_t> EmbeddedController::read_byte(uint8_t offset)
	{
		uint8_t response;
		Command command {
			.command = 0x80,
			.data1 = offset,
			.data2 = {},
			.response = &response,
			.done = false,
		};

		TRY(send_command(command));

		return response;
	}

	BAN::ErrorOr<void> EmbeddedController::write_byte(uint8_t offset, uint8_t value)
	{
		Command command {
			.command = 0x81,
			.data1 = offset,
			.data2 = value,
			.response = nullptr,
			.done = false,
		};

		TRY(send_command(command));

		return {};
	}

	uint8_t EmbeddedController::read_one(uint16_t port)
	{
		wait_status_bit(STS_OBF, 1);
		return IO::inb(port);
	}

	void EmbeddedController::write_one(uint16_t port, uint8_t value)
	{
		wait_status_bit(STS_IBF, 0);
		IO::outb(port, value);
	}

	void EmbeddedController::wait_status_bit(uint8_t bit, uint8_t value)
	{
		// FIXME: timeouts
		const uint8_t mask = 1 << bit;
		const uint8_t comp = value ? mask : 0;
		while ((IO::inb(m_command_port) & mask) != comp)
			continue;
	}

	void EmbeddedController::handle_gpe_wrapper(void* embedded_controller)
	{
		static_cast<EmbeddedController*>(embedded_controller)->handle_gpe();
	}

	void EmbeddedController::handle_gpe()
	{
		for (;;)
		{
			if (!(IO::inb(m_command_port) & STS_SCI_EVT))
				break;

			uint8_t response;
			Command command = {
				.command = 0x84,
				.data1 = {},
				.data2 = {},
				.response = &response,
				.done = false,
			};

			if (auto ret = send_command(command); ret.is_error())
			{
				dwarnln("Failed to send SC_QUERY: {}", ret.error());
				break;
			}

			if (response == 0)
			{
				dprintln("  No query");
				break;
			}

			if (auto ret = call_query_method(response); ret.is_error())
				dwarnln("Failed to call _Q{2H}: {}", response, ret.error());
		}
	}

	BAN::ErrorOr<void> EmbeddedController::call_query_method(uint8_t notification)
	{
		const auto nibble_to_hex_char =
			[](uint8_t nibble) -> char
			{
				if (nibble < 10)
					return '0' + nibble;
				return 'A' + nibble - 10;
			};

		const char query_method[] {
			'_', 'Q',
			nibble_to_hex_char(notification >> 4),
			nibble_to_hex_char(notification & 0xF),
			'\0'
		};

		auto [method_path, method_obj] = TRY(ACPI::get().acpi_namespace()->find_named_object(m_scope, TRY(AML::NameString::from_string(query_method)), true));
		if (method_obj == nullptr)
		{
			dwarnln("{} not found", query_method);
			return {};
		}
		const auto& method = method_obj->node;

		if (method.type != AML::Node::Type::Method)
		{
			dwarnln("{} is not a method", query_method);
			return {};
		}
		if (method.as.method.arg_count != 0)
		{
			dwarnln("{} takes {} arguments", query_method, method.as.method.arg_count);
			return {};
		}

		TRY(AML::method_call(method_path, method));
		return {};
	}

	BAN::ErrorOr<void> EmbeddedController::send_command(Command& command)
	{
		LockGuard _(m_mutex);

		const uint64_t wake_time_ms = SystemTimer::get().ms_since_boot() + 1000;
		while (m_queued_command.has_value() && SystemTimer::get().ms_since_boot() <= wake_time_ms)
			m_thread_blocker.block_with_wake_time_ms(wake_time_ms, &m_mutex);
		if (m_queued_command.has_value())
			return BAN::Error::from_errno(ETIMEDOUT);

		m_queued_command = &command;
		m_thread_blocker.unblock();

		while (!command.done)
			m_thread_blocker.block_indefinite(&m_mutex);

		return {};
	}

	void EmbeddedController::thread_task()
	{
		m_mutex.lock();

		for (;;)
		{
			Command* const command = m_queued_command.has_value() ? m_queued_command.value() : nullptr;
			m_queued_command.clear();

			if (command == nullptr)
			{
				m_thread_blocker.block_indefinite(&m_mutex);
				continue;
			}

			m_mutex.unlock();

			if (command)
			{
				// TODO: use burst mode

				write_one(m_command_port, command->command);
				if (command->data1.has_value())
					write_one(m_data_port, command->data1.value());
				if (command->data2.has_value())
					write_one(m_data_port, command->data2.value());
				if (command->response)
					*command->response = read_one(m_data_port);

				m_mutex.lock();
				command->done = true;
				m_thread_blocker.unblock();
				m_mutex.unlock();
			}

			m_mutex.lock();
		}
	}

}
