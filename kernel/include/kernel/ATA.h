#pragma once

#include <BAN/Vector.h>
#include <kernel/DiskIO.h>

namespace Kernel
{

	class ATADevice : public DiskDevice
	{
	public:
		static ATADevice* create(uint16_t io_base, uint16_t ctl_base, uint8_t slave_bit);

		virtual ~ATADevice() {}

		uint16_t io_base() const { return m_io_base; }
		uint16_t ctl_base() const { return m_ctl_base; }
		uint8_t slave_bit() const { return m_slave_bit; }
		virtual const char* type() const = 0;
	
	protected:
		ATADevice(uint16_t io_base, uint16_t ctl_base, uint8_t slave_bit)
			: m_io_base(io_base)
			, m_ctl_base(ctl_base)
			, m_slave_bit(slave_bit)
		{}

	private:
		const uint16_t m_io_base;
		const uint16_t m_ctl_base;
		const uint8_t m_slave_bit;
	};

	class PATADevice final : public ATADevice
	{
	public:
		PATADevice(uint16_t io_base, uint16_t ctl_base, uint8_t slave_bit)
			: ATADevice(io_base, ctl_base, slave_bit)
		{}

		virtual const char* type() const override { return "PATA"; }
		virtual bool read(uint32_t lba, uint32_t sector_count, uint8_t* buffer) override;

	protected:
		virtual bool initialize() override;

	private:
		bool read_lba28(uint32_t lba, uint8_t sector_count, uint8_t* buffer);
		bool wait_while_buzy();
		bool wait_for_transfer();
		void flush();

	private:
		bool m_lba_48 = false;
	};
	

}
