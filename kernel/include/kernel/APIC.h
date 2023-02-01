#pragma once

#include <BAN/Vector.h>
#include <kernel/InterruptController.h>

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
		uintptr_t address;
		uint32_t gsi_base;
		uint8_t max_redirs;

		uint32_t read(uint8_t offset);
		void write(uint8_t offset, uint32_t data);
	};

private:
	BAN::Vector<Processor>	m_processors;
	uintptr_t				m_local_apic = 0;
	BAN::Vector<IOAPIC>		m_io_apics;	
	uint8_t					m_irq_overrides[0x100] {};
};