.section .text

.global _start
_start:
	pushl $0
	pushl %edx
	pushl %esi
	pushl %edi

	# STACK LAYOUT
	#   null
	#   argc
	#   argv
	#   envp

	xorl %ebp, %ebp

	pushl $__fini_array_end
	pushl $__fini_array_start
	pushl $_fini

	pushl $__init_array_end
	pushl $__init_array_start
	pushl $_init

	pushl %edx

	call _init_libc

	addl $(4 * 8), %esp

	# argc, argv, envp already on stack
	call main

	subl $12, %esp
	pushl %eax
	call exit

.size _start, . - _start
