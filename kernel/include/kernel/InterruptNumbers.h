#pragma once

#include <kernel/Arch.h>

namespace Kernel
{

	// IDT entries
	//   0x00->0x1F (32):  ISR
	//   0x20->0x7F (96):  PIC/IOAPIC
	//   0x80->0xEF (112): MSI
	//   0xF0->0xFE (15):  internal

	constexpr uint8_t IRQ_VECTOR_BASE   = 0x20;
	constexpr uint8_t IRQ_MSI_BASE      = 0x80;
	constexpr uint8_t IRQ_MSI_END       = 0xF0;
#if ARCH(i686)
	constexpr uint8_t IRQ_SYSCALL       = 0xF0; // hard coded in kernel/API/Syscall.h
#endif
	constexpr uint8_t IRQ_IPI           = 0xF1;
	constexpr uint8_t IRQ_TIMER         = 0xF2;

}
