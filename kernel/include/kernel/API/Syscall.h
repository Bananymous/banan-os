#pragma once

#include <BAN/MacroUtils.h>

#if defined(__x86_64__)
	#define _kas_instruction "syscall"
	#define _kas_result rax
	#define _kas_arguments rdi, rsi, rdx, r10, r8, r9
	#define _kas_globbers rcx, rdx, rdi, rsi, r8, r9, r10, r11
#elif defined(__i686__)
	#define _kas_instruction "int $0xF0"
	#define _kas_result eax
	#define _kas_arguments eax, ebx, ecx, edx, esi, edi
	#define _kas_globbers
#endif

#define _kas_argument_var(index, value) register long _kas_a##index asm(_ban_stringify(_ban_get(index, _kas_arguments))) = (long)(value);
#define _kas_dummy_var(index, value) register long _kas_d##index asm(#value);
#define _kas_input(index, _) "r"(_kas_a##index)
#define _kas_output(index, _) , "=r"(_kas_d##index)
#define _kas_globber(_, value) #value

#define _kas_syscall(...) ({ \
		register long _kas_ret asm(_ban_stringify(_kas_result)); \
		_ban_for_each(_kas_argument_var, __VA_ARGS__) \
		_ban_for_each(_kas_dummy_var, _kas_globbers) \
		asm volatile( \
			_kas_instruction \
			: "=r"(_kas_ret) _ban_for_each(_kas_output, _kas_globbers) \
			: _ban_for_each_comma(_kas_input, __VA_ARGS__) \
			: "cc", "memory"); \
		(void)_kas_a0; /* require 1 argument */ \
		_kas_ret; \
	})
