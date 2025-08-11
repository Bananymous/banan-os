#pragma once

#include <BAN/Atomic.h>
#include <BAN/UniqPtr.h>

#include <kernel/ACPI/AML/Scope.h>
#include <kernel/Lock/Mutex.h>
#include <kernel/Thread.h>

namespace Kernel::ACPI
{

	class EmbeddedController
	{
	public:
		static BAN::ErrorOr<BAN::UniqPtr<EmbeddedController>> create(AML::Scope&& scope, uint16_t command_port, uint16_t data_port, BAN::Optional<uint8_t> gpe);
		~EmbeddedController();

		BAN::ErrorOr<uint8_t> read_byte(uint8_t offset);
		BAN::ErrorOr<void> write_byte(uint8_t offset, uint8_t value);

		const AML::Scope& scope() const { return m_scope; }

	private:
		EmbeddedController(AML::Scope&& scope, uint16_t command_port, uint16_t data_port, bool has_gpe)
			: m_scope(BAN::move(scope))
			, m_command_port(command_port)
			, m_data_port(data_port)
			, m_has_gpe(has_gpe)
		{ }

	private:
		void wait_status_bit(uint8_t bit, uint8_t value);

		uint8_t read_one(uint16_t port);
		void write_one(uint16_t port, uint8_t value);

		static void handle_gpe_wrapper(void*);
		void handle_gpe();

		BAN::ErrorOr<void> call_query_method(uint8_t notification);

		void thread_task();

		struct Command
		{
			uint8_t command;
			BAN::Optional<uint8_t> data1;
			BAN::Optional<uint8_t> data2;
			uint8_t* response;
			BAN::Atomic<bool> done;
		};
		BAN::ErrorOr<void> send_command(Command& command);

	private:
		const AML::Scope m_scope;
		const uint16_t m_command_port;
		const uint16_t m_data_port;
		const bool m_has_gpe;

		Mutex m_mutex;
		ThreadBlocker m_thread_blocker;

		BAN::Optional<Command*> m_queued_command;

		Thread* m_thread { nullptr };
	};

}
