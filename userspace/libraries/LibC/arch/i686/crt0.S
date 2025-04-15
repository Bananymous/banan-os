.section .text

.global _start
_start:
	# get argc, argv, envp
	movl  (%esp), %edi
	leal 4(%esp), %esi
	leal 4(%esi, %edi, 4), %edx

	# align stack
	andl $-16, %esp
	xorl %ebp, %ebp

	# save argc, argv, envp
	subl $4, %esp
	pushl %edx
	pushl %esi
	pushl %edi

	subl $4, %esp

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
