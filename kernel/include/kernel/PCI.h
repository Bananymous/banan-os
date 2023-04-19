#pragma once

#include <BAN/Vector.h>

namespace Kernel
{

	class PCIDevice
	{
	public:
		PCIDevice(uint8_t, uint8_t, uint8_t);

		uint32_t read_dword(uint8_t) const;
		uint16_t read_word(uint8_t) const;
		uint8_t  read_byte(uint8_t) const;

		uint8_t bus() const { return m_bus; }
		uint8_t dev() const { return m_dev; }
		uint8_t func() const { return m_func; }

		uint8_t class_code() const { return m_class_code; }
		uint8_t subclass() const { return m_subclass; }
		uint8_t prog_if() const { return m_prog_if; }

	private:
		uint8_t m_bus;
		uint8_t m_dev;
		uint8_t m_func;

		uint8_t m_class_code;
		uint8_t m_subclass;
		uint8_t m_prog_if;
	};

	class PCI
	{
		BAN_NON_COPYABLE(PCI);
		BAN_NON_MOVABLE(PCI);

	public:
		static void initialize();
		static PCI& get();
		
		const BAN::Vector<PCIDevice>& devices() const { return m_devices; }

	private:
		PCI() = default;
		void check_function(uint8_t bus, uint8_t dev, uint8_t func);
		void check_device(uint8_t bus, uint8_t dev);
		void check_bus(uint8_t bus);
		void check_all_buses();

	private:
		BAN::Vector<PCIDevice> m_devices;
	};

}