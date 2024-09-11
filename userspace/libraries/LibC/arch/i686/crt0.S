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
	movl $_init, %eax
	testl %eax, %eax
	jz 1f
	call *%eax
 1:

	movl $__init_array_start, %ebx
	jmp 2f
 1: call *(%ebx)
	addl $4, %ebx
 2: cmpl $__init_array_end, %ebx
	jne 1b

	# call main
	movl 0(%esp), %eax
	xchgl %eax, 8(%esp)
	movl %eax, (%esp)
	call main

	subl $12, %esp
	pushl %eax
	call exit

.size _start, . - _start
