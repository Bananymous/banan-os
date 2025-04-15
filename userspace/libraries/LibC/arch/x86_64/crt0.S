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

	pushq $__fini_array_end
	pushq $__fini_array_start
	pushq $_fini

	pushq $__init_array_end
	pushq $__init_array_start
	pushq $_init

	call _init_libc

	addq $(6 * 8), %rsp

	# call main
	movq 16(%rsp), %rdi
	movq  8(%rsp), %rsi
	movq  0(%rsp), %rdx
	call main

	# call exit
	movq %rax, %rdi
	call exit

.size _start, . - _start
