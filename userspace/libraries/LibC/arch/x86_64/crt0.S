.section .text

.global _start
_start:
	pushq $0
	pushq %rdi
	pushq %rsi
	pushq %rdx

	# STACK LAYOUT
	#   null
	#   argc
	#   argv
	#   envp

	xorq %rbp, %rbp

	# init libc
	movq 0(%rsp), %rdi
	call _init_libc

	# call global constructors
	movq $_init, %rax
	testq %rax, %rax
	jz 1f
	call *%rax
 1:

	movq $__init_array_start, %rbx
	jmp 2f
 1: call *(%rbx)
	addq $8, %rbx
 2: cmpq $__init_array_end, %rbx
	jne 1b

	# call main
	movq 16(%rsp), %rdi
	movq  8(%rsp), %rsi
	movq  0(%rsp), %rdx
	call main

	# call exit
	movq %rax, %rdi
	call exit

.size _start, . - _start
