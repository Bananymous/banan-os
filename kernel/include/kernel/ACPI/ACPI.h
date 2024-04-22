#pragma once

#include <BAN/Vector.h>
#include <kernel/ACPI/AML/Method.h>
#include <kernel/ACPI/AML/Namespace.h>
#include <kernel/ACPI/Headers.h>
#include <kernel/Memory/Types.h>

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

		// This function will power off the system
		// This function will return only if there was an error
		void poweroff();

		// This function will reset the system
		// This function will return only if there was an error
		void reset();

		void handle_irq() override;

	private:
		ACPI() = default;
		BAN::ErrorOr<void> initialize_impl();

		FADT& fadt() { return *m_fadt; }

		bool prepare_sleep(uint8_t sleep_state);
		void acpi_event_task();

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

		Semaphore m_event_semaphore;
		BAN::Array<BAN::RefPtr<AML::Method>, 0xFF> m_gpe_methods;

		bool m_hardware_reduced { false };
		BAN::RefPtr<AML::Namespace> m_namespace;
	};

}
