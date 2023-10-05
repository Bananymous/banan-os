#pragma once

#include <BAN/Vector.h>
#include <kernel/InterruptController.h>
#include <kernel/Memory/Types.h>

namespace Kernel
{

	class APIC final : public InterruptController
	{
	public:
		virtual void eoi(uint8_t) override;
		virtual void enable_irq(uint8_t) override;
		virtual bool is_in_service(uint8_t) override;

	private:
		uint32_t read_from_local_apic(ptrdiff_t);
		void write_to_local_apic(ptrdiff_t, uint32_t);

	private:
		~APIC() { ASSERT_NOT_REACHED(); }
		static APIC* create();
		friend class InterruptController;

	private:
		struct Processor
		{
			enum Flags : uint8_t
			{
				Enabled = 1,
				OnlineCapable = 2,
			};
			uint8_t processor_id;
			uint8_t apic_id;
			uint8_t flags;
		};

		struct IOAPIC
		{
			uint8_t id;
			Kernel::paddr_t paddr;
			Kernel::vaddr_t vaddr;
			uint32_t gsi_base;
			uint8_t max_redirs;

			uint32_t read(uint8_t offset);
			void write(uint8_t offset, uint32_t data);
		};

	private:
		BAN::Vector<Processor>	m_processors;
		Kernel::paddr_t			m_local_apic_paddr = 0;
		Kernel::vaddr_t			m_local_apic_vaddr = 0;
		BAN::Vector<IOAPIC>		m_io_apics;	
		uint8_t					m_irq_overrides[0x100] {};
	};

}
