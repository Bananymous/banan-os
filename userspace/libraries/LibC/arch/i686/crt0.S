.section .text

.global _start
_start:
	pushl $0
	pushl %edi
	pushl %esi
	pushl %edx

	# STACK LAYOUT
	#   null
	#   argc
	#   argv
	#   envp

	xorl %ebp, %ebp

	# init libc (envp already as argument)
	call _init_libc

	# call global constructors
	call _init

	# call main
	movl 0(%esp), %eax
	xchgl %eax, 8(%esp)
	movl %eax, (%esp)
	call main

	subl $12, %esp
	pushl %eax
	call exit

.size _start, . - _start
