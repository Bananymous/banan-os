.section .text

.global _start
_start:
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
	call _init

	# call main
	movq 16(%rsp), %rdi
	movq  8(%rsp), %rsi
	movq  0(%rsp), %rdx
	call main

	# call exit
	movq %rax, %rdi
	call exit

.size _start, . - _start
