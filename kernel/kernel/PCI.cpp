#include <kernel/IO.h>
#include <kernel/PCI.h>

#define INVALID 0xFFFF
#define MULTI_FUNCTION 0x80

#define CONFIG_ADDRESS 0xCF8
#define CONFIG_DATA 0xCFC

namespace Kernel
{

	static PCI* s_instance = nullptr;

	void PCI::initialize()
	{
		ASSERT(s_instance == nullptr);
		s_instance = new PCI();
		ASSERT(s_instance);
		s_instance->check_all_buses();
	}

	PCI& PCI::get()
	{
		ASSERT(s_instance);
		return *s_instance;
	}

	static uint32_t read_config_dword(uint8_t bus, uint8_t dev, uint8_t func, uint8_t offset)
	{
		uint32_t config_addr = 0x80000000 | ((uint32_t)bus << 16) | ((uint32_t)dev << 11) | ((uint32_t)func << 8) | offset;
		IO::outl(CONFIG_ADDRESS, config_addr);
		return IO::inl(CONFIG_DATA);
	}

	static uint16_t get_vendor_id(uint8_t bus, uint8_t dev, uint8_t func)
	{
		uint32_t dword = read_config_dword(bus, dev, func, 0x00);
		return dword & 0xFFFF;
	}

	static uint8_t get_header_type(uint8_t bus, uint8_t dev, uint8_t func)
	{
		uint32_t dword = read_config_dword(bus, dev, func, 0x0C);
		return (dword >> 16) & 0xFF;
	}

	void PCI::check_function(uint8_t bus, uint8_t dev, uint8_t func)
	{
		MUST(m_devices.emplace_back(bus, dev, func));
		auto& device = m_devices.back();
		if (device.class_code() == 0x06 && device.subclass() == 0x04)
			check_bus(device.read_byte(0x19));
	}

	void PCI::check_device(uint8_t bus, uint8_t dev)
	{
		if (get_vendor_id(bus, dev, 0) == INVALID)
			return;
		
		check_function(bus, dev, 0);
		if (get_header_type(bus, dev, 0) & MULTI_FUNCTION)
			for (uint8_t func = 1; func < 8; func++)
				if (get_vendor_id(bus, dev, func) != INVALID)
					check_function(bus, dev, func);
	}

	void PCI::check_bus(uint8_t bus)
	{
		for (uint8_t dev = 0; dev < 32; dev++)
			check_device(bus, dev);
	}

	void PCI::check_all_buses()
	{
		if (get_header_type(0, 0, 0) & MULTI_FUNCTION)
		{
			for (int func = 0; func < 8 && get_vendor_id(0, 0, func) != INVALID; func++)
				check_bus(func);
		}
		else
		{
			check_bus(0);
		}
	}

	PCIDevice::PCIDevice(uint8_t bus, uint8_t dev, uint8_t func)
		: m_bus(bus), m_dev(dev), m_func(func)
	{
		uint32_t type = read_word(0x0A);
		m_class_code  = (uint8_t)(type >> 8);
		m_subclass    = (uint8_t)(type);
		m_prog_if     = read_byte(0x09);
	}

	uint32_t PCIDevice::read_dword(uint8_t offset) const
	{
		ASSERT((offset & 0x03) == 0);
		return read_config_dword(m_bus, m_dev, m_func, offset);
	}

	uint16_t PCIDevice::read_word(uint8_t offset) const
	{
		ASSERT((offset & 0x01) == 0);
		uint32_t dword = read_config_dword(m_bus, m_dev, m_func, offset & 0xFC);
		return (uint16_t)(dword >> (8 * (offset & 0x03)));
	}

	uint8_t PCIDevice::read_byte(uint8_t offset) const
	{
		uint32_t dword = read_config_dword(m_bus, m_dev, m_func, offset & 0xFC);
		return (uint8_t)(dword >> (8 * (offset & 0x03)));
	}

}