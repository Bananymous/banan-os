#pragma once

#define x86_64 1
#define i686   2

#define ARCH(arch) (__arch == arch)

#if !defined(__arch) || (__arch != x86_64 && __arch != i686)
	#error "Unsupported architecture"
#endif

#if ARCH(x86_64)
	#define read_rsp(rsp) asm volatile("movq %%rsp, %0" : "=r"(rsp))
	#define push_callee_saved() asm volatile("pushq %rbx; pushq %rbp; pushq %r12; pushq %r13; pushq %r14; pushq %r15")
	#define pop_callee_saved()  asm volatile("popq %r15; popq %r14; popq %r13; popq %r12; popq %rbp; popq %rbx")
#elif ARCH(i686)
	#define read_rsp(rsp) asm volatile("movl %%esp, %0" : "=r"(rsp))
	#define push_callee_saved() asm volatile("pushal")
	#define pop_callee_saved() asm volatile("popal")
#else
	#error
#endif

#include <stdint.h>

extern "C" uintptr_t read_ip();
