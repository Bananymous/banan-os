#pragma once

#include <stdint.h>

namespace Kernel
{

	struct InterruptStack
	{
		uintptr_t ip;
		uintptr_t cs;
		uintptr_t flags;
		uintptr_t sp;
		uintptr_t ss;
	};

	struct InterruptRegisters
	{
		uintptr_t r15;
		uintptr_t r14;
		uintptr_t r13;
		uintptr_t r12;
		uintptr_t r11;
		uintptr_t r10;
		uintptr_t r9;
		uintptr_t r8;

		uintptr_t rdi;
		uintptr_t rsi;
		uintptr_t rbp;
		uintptr_t rbx;
		uintptr_t rdx;
		uintptr_t rcx;
		uintptr_t rax;
	};

}
