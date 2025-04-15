.section .text

.global _start
_start:
	# get argc, argv, envp
	movq  (%rsp), %rdi
	leaq 8(%rsp), %rsi
	leaq 8(%rsi, %rdi, 8), %rdx

	# align stack
	andq $-16, %rsp
	xorq %rbp, %rbp

	# save argc, argv, envp
	subq $8, %rsp
	pushq %rdi
	pushq %rsi
	pushq %rdx

	movq %rdx, %rdi # environ

	pushq $__fini_array_end
	pushq $__fini_array_start
	pushq $_fini

	pushq $__init_array_end
	pushq $__init_array_start
	pushq $_init

	call _init_libc

	addq $(6 * 8), %rsp

	# call main
	popq %rdx
	popq %rsi
	popq %rdi
	addq $8, %rsp
	call main

	# call exit
	movq %rax, %rdi
	call exit

.size _start, . - _start
