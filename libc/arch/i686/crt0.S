.section .text

.global _start
_start:
	# zero out stack frame
	pushl $0
	pushl $0
	movl %esp, %ebp

	# FIXME: handle stack alignment
	ud2

	# push argc, argv, environ for call to main
	pushl %edx
	pushl %esi
	pushl %edi

	# initialize libc
	pushl %edx
	call _init_libc
	addl $4, %esp

	# call global constructos
	call _init

	# call main, arguments are already on stack
	call main

	# cleanly exit the process
	pushl %eax
	call exit

.size _start, . - _start
