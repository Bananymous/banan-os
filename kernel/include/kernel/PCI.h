#pragma once

#include <BAN/UniqPtr.h>
#include <BAN/Vector.h>
#include <kernel/Memory/Types.h>
#include <kernel/Storage/StorageController.h>

#include <sys/types.h>

namespace Kernel::PCI
{

	enum class BarType
	{
		INVALID,
		MEM,
		IO,
	};

	class Device;

	class BarRegion
	{
		BAN_NON_COPYABLE(BarRegion);
		BAN_NON_MOVABLE(BarRegion);

	public:
		static BAN::ErrorOr<BAN::UniqPtr<BarRegion>> create(PCI::Device&, uint8_t bar_num);
		~BarRegion();

		BarType type() const { return m_type; }
		vaddr_t iobase() const { ASSERT(m_type == BarType::IO); return m_paddr; }
		vaddr_t vaddr() const { ASSERT(m_type == BarType::MEM); return m_vaddr; }
		paddr_t paddr() const { ASSERT(m_type == BarType::MEM); return m_paddr; }
		size_t size() const { return m_size; }

		void write8(off_t, uint8_t);
		void write16(off_t, uint16_t);
		void write32(off_t, uint32_t);

		uint8_t read8(off_t);
		uint16_t read16(off_t);
		uint32_t read32(off_t);

	private:
		BarRegion(BarType, paddr_t, size_t);
		BAN::ErrorOr<void> initialize();

	private:
		const BarType	m_type	{};
		const paddr_t	m_paddr	{};
		const size_t	m_size	{};
		vaddr_t			m_vaddr	{};
	};

	class Device
	{
	public:
		Device(uint8_t, uint8_t, uint8_t);

		uint32_t read_dword(uint8_t) const;
		uint16_t read_word(uint8_t) const;
		uint8_t  read_byte(uint8_t) const;

		void write_dword(uint8_t, uint32_t);
		void write_word(uint8_t, uint16_t);
		void write_byte(uint8_t, uint8_t);

		uint8_t bus() const { return m_bus; }
		uint8_t dev() const { return m_dev; }
		uint8_t func() const { return m_func; }

		uint8_t class_code() const { return m_class_code; }
		uint8_t subclass() const { return m_subclass; }
		uint8_t prog_if() const { return m_prog_if; }

		uint8_t header_type() const { return m_header_type; }

		uint16_t vendor_id() const { return m_vendor_id; }
		uint16_t device_id() const { return m_device_id; }

		BAN::ErrorOr<void> reserve_irqs(uint8_t count);
		uint8_t get_irq(uint8_t index);

		BAN::ErrorOr<BAN::UniqPtr<BarRegion>> allocate_bar_region(uint8_t bar_num);

		void enable_bus_mastering();
		void disable_bus_mastering();

		void enable_memory_space();
		void disable_memory_space();

		void enable_io_space();
		void disable_io_space();

	private:
		void enumerate_capabilites();

		void set_command_bits(uint16_t mask);
		void unset_command_bits(uint16_t mask);

		void enable_pin_interrupts();
		void disable_pin_interrupts();

	private:
		const uint8_t m_bus;
		const uint8_t m_dev;
		const uint8_t m_func;

		uint8_t m_class_code;
		uint8_t m_subclass;
		uint8_t m_prog_if;

		uint8_t m_header_type;
		uint16_t m_vendor_id;
		uint16_t m_device_id;

		uint32_t m_reserved_irqs { 0 };
		uint8_t m_reserved_irq_count { 0 };

		BAN::Optional<uint8_t> m_offset_msi;
		BAN::Optional<uint8_t> m_offset_msi_x;
	};

	class PCIManager
	{
		BAN_NON_COPYABLE(PCIManager);
		BAN_NON_MOVABLE(PCIManager);

	public:
		static void initialize();
		static PCIManager& get();

		const BAN::Vector<PCI::Device>& devices() const { return m_devices; }

		static uint32_t read_config_dword(uint8_t bus, uint8_t dev, uint8_t func, uint8_t offset);
		static uint16_t read_config_word(uint8_t bus, uint8_t dev, uint8_t func, uint8_t offset);
		static uint8_t read_config_byte(uint8_t bus, uint8_t dev, uint8_t func, uint8_t offset);

		static void write_config_dword(uint8_t bus, uint8_t dev, uint8_t func, uint8_t offset, uint32_t value);
		static void write_config_word(uint8_t bus, uint8_t dev, uint8_t func, uint8_t offset, uint16_t value);
		static void write_config_byte(uint8_t bus, uint8_t dev, uint8_t func, uint8_t offset, uint8_t value);

	private:
		PCIManager() = default;
		void check_function(uint8_t bus, uint8_t dev, uint8_t func);
		void check_device(uint8_t bus, uint8_t dev);
		void check_bus(uint8_t bus);
		void check_all_buses();
		void initialize_devices();

	private:
		BAN::Vector<PCI::Device> m_devices;
	};

}
