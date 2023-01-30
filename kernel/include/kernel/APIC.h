#pragma once

#include <BAN/Vector.h>
#include <kernel/InterruptController.h>

class APIC final : public InterruptController
{
public:
	virtual void EOI(uint8_t) override;
	virtual void EnableIrq(uint8_t) override;
	virtual bool IsInService(uint8_t) override;

private:
	uint32_t ReadFromLocalAPIC(ptrdiff_t);
	void WriteToLocalAPIC(ptrdiff_t, uint32_t);

private:
	static APIC* Create();
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

		uint32_t Read(uint8_t offset);
		void Write(uint8_t offset, uint32_t data);
	};

private:
	BAN::Vector<Processor>	m_processors;
	uintptr_t				m_local_apic = 0;
	BAN::Vector<IOAPIC>		m_io_apics;	
	uint8_t					m_irq_overrides[0x100] {};
};