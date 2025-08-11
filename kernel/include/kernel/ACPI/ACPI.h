#pragma once

#include <BAN/Vector.h>
#include <kernel/ACPI/AML/Namespace.h>
#include <kernel/ACPI/EmbeddedController.h>
#include <kernel/ACPI/Headers.h>
#include <kernel/Memory/Types.h>
#include <kernel/ThreadBlocker.h>

namespace Kernel::ACPI
{

	class ACPI : public Interruptable
	{
	public:
		static BAN::ErrorOr<void> initialize();
		static ACPI& get();

		static void acquire_global_lock();
		static void release_global_lock();

		bool hardware_reduced() const { return m_hardware_reduced; }

		const SDTHeader* get_header(BAN::StringView signature, uint32_t index);

		// mode
		//   0: PIC
		//   1: APIC
		//   2: SAPIC
		BAN::ErrorOr<void> enter_acpi_mode(uint8_t mode);

		BAN::ErrorOr<void> initialize_acpi_devices();

		AML::Namespace* acpi_namespace() { return m_namespace; }

		BAN::ErrorOr<void> poweroff();
		BAN::ErrorOr<void> reset();

		BAN::ErrorOr<void> register_gpe_handler(uint8_t gpe, void (*callback)(void*), void* argument);

		void handle_irq() override;

		BAN::Span<BAN::UniqPtr<EmbeddedController>> embedded_controllers() { return m_embedded_controllers.span(); }

	private:
		ACPI() = default;
		BAN::ErrorOr<void> initialize_impl();

		FADT& fadt() { return *m_fadt; }

		BAN::ErrorOr<void> prepare_sleep(uint8_t sleep_state);
		void acpi_event_task();

		BAN::ErrorOr<void> load_aml_tables(BAN::StringView name, bool all);

		BAN::ErrorOr<void> route_interrupt_link_device(const AML::Scope& device, uint64_t& routed_irq_mask);

		BAN::ErrorOr<void> initialize_embedded_controller(const AML::Scope& embedded_controller);
		BAN::ErrorOr<void> initialize_embedded_controllers();

		BAN::Optional<GAS> find_gpe_block(size_t index);
		bool enable_gpe(uint8_t gpe);

	private:
		paddr_t m_header_table_paddr = 0;
		vaddr_t m_header_table_vaddr = 0;
		uint32_t m_entry_size = 0;

		struct MappedPage
		{
			Kernel::paddr_t paddr;
			Kernel::vaddr_t vaddr;

			SDTHeader* as_header() { return (SDTHeader*)vaddr; }
		};
		BAN::Vector<MappedPage> m_mapped_headers;

		FADT* m_fadt { nullptr };

		ThreadBlocker m_event_thread_blocker;

		BAN::Vector<BAN::UniqPtr<EmbeddedController>> m_embedded_controllers;

		struct GPEHandler
		{
			bool has_callback { false };
			union {
				AML::Reference* method;
				struct
				{
					void (*callback)(void*);
					void* argument;
				};
			};
		};
		bool m_has_any_gpes { false };
		AML::Scope m_gpe_scope;
		BAN::Array<GPEHandler, 0xFF> m_gpe_methods;

		bool m_hardware_reduced { false };
		AML::Namespace* m_namespace { nullptr };
	};

}
