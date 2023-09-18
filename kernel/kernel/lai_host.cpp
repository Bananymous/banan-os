#include <kernel/ACPI.h>
#include <kernel/IO.h>
#include <kernel/Memory/kmalloc.h>
#include <kernel/Memory/PageTable.h>
#include <kernel/PCI.h>
#include <kernel/Timer/Timer.h>

#include <lai/host.h>

using namespace Kernel;

void* laihost_malloc(size_t size)
{
	return kmalloc(size);
}

void* laihost_realloc(void* ptr, size_t newsize, size_t oldsize)
{
	if (ptr == nullptr)
		return laihost_malloc(newsize);

	void* new_ptr = laihost_malloc(newsize);
	if (new_ptr == nullptr)
		return nullptr;

	memcpy(new_ptr, ptr, BAN::Math::min(newsize, oldsize));
	kfree(ptr);

	return new_ptr;
}

void laihost_free(void* ptr, size_t)
{
	kfree(ptr);
}

void laihost_log(int level, const char* msg)
{
	if (level == LAI_DEBUG_LOG)
		dprintln(msg);
	else if (level == LAI_WARN_LOG)
		dwarnln(msg);
	else
		ASSERT_NOT_REACHED();
}

void laihost_panic(const char* msg)
{
	Kernel::panic(msg);
}

void* laihost_scan(const char* sig, size_t index)
{
	ASSERT(index == 0);
	return (void*)ACPI::get().get_header(sig);
}

void* laihost_map(size_t address, size_t count)
{
	size_t needed_pages = range_page_count(address, count);
	vaddr_t vaddr = PageTable::kernel().reserve_free_contiguous_pages(needed_pages, KERNEL_OFFSET);
	ASSERT(vaddr);

	PageTable::kernel().map_range_at(address & PAGE_ADDR_MASK, vaddr, needed_pages * PAGE_SIZE, PageTable::Flags::ReadWrite | PageTable::Flags::Present);

	return (void*)(vaddr + (address % PAGE_SIZE));
}

void laihost_unmap(void* ptr, size_t count)
{
	PageTable::kernel().unmap_range((vaddr_t)ptr, count);
}

void laihost_outb(uint16_t port, uint8_t val)
{
	IO::outb(port, val);
}

void laihost_outw(uint16_t port, uint16_t val)
{
	IO::outw(port, val);
}

void laihost_outd(uint16_t port, uint32_t val)
{
	IO::outl(port, val);
}

uint8_t laihost_inb(uint16_t port)
{
	return IO::inb(port);
}

uint16_t laihost_inw(uint16_t port)
{
	return IO::inw(port);
}

uint32_t laihost_ind(uint16_t port)
{
	return IO::inl(port);
}

void laihost_pci_writeb(uint16_t seg, uint8_t bus, uint8_t slot, uint8_t fun, uint16_t offset, uint8_t val)
{
	ASSERT(seg == 0);
	PCI::PCIManager::write_config_byte(bus, slot, fun, offset, val);
}

void laihost_pci_writew(uint16_t seg, uint8_t bus, uint8_t slot, uint8_t fun, uint16_t offset, uint16_t val)
{
	ASSERT(seg == 0);
	PCI::PCIManager::write_config_word(bus, slot, fun, offset, val);
}

void laihost_pci_writed(uint16_t seg, uint8_t bus, uint8_t slot, uint8_t fun, uint16_t offset, uint32_t val)
{
	ASSERT(seg == 0);
	PCI::PCIManager::write_config_dword(bus, slot, fun, offset, val);
}

uint8_t laihost_pci_readb(uint16_t seg, uint8_t bus, uint8_t slot, uint8_t fun, uint16_t offset)
{
	ASSERT(seg == 0);
	return PCI::PCIManager::read_config_byte(bus, slot, fun, offset);
}

uint16_t laihost_pci_readw(uint16_t seg, uint8_t bus, uint8_t slot, uint8_t fun, uint16_t offset)
{
	ASSERT(seg == 0);
	return PCI::PCIManager::read_config_word(bus, slot, fun, offset);

}

uint32_t laihost_pci_readd(uint16_t seg, uint8_t bus, uint8_t slot, uint8_t fun, uint16_t offset)
{
	ASSERT(seg == 0);
	return PCI::PCIManager::read_config_dword(bus, slot, fun, offset);
}

void laihost_sleep(uint64_t ms)
{
	SystemTimer::get().sleep(ms);
}

uint64_t laihost_timer(void)
{
	auto time = SystemTimer::get().time_since_boot();
	return (1'000'000'000ull * time.tv_sec + time.tv_nsec) / 100;
}
