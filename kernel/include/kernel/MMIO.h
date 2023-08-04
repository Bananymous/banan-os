#pragma once

#include <stdint.h>
#include <stddef.h>

namespace MMIO
{

	static inline void write8(uintptr_t addr, uint8_t val)
	{
		*(volatile uint8_t*)addr = val;
	}

	static inline void write16(uintptr_t addr, uint16_t val)
	{
		*(volatile uint16_t*)addr = val;
	}

	static inline void write32(uintptr_t addr, uint32_t val)
	{
		*(volatile uint32_t*)addr = val;
	}

	static inline void write64(uintptr_t addr, uint64_t val)
	{
		*(volatile uint64_t*)addr = val;
	}

	static inline uint8_t read8(uintptr_t addr)
	{
		return *(volatile uint8_t*)addr;
	}

	static inline uint16_t read16(uintptr_t addr)
	{
		return *(volatile uint16_t*)addr;
	}

	static inline uint32_t read32(uintptr_t addr)
	{
		return *(volatile uint32_t*)addr;
	}

	static inline uint64_t read64(uintptr_t addr)
	{
		return *(volatile uint64_t*)addr;
	}

}