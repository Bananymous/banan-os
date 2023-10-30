.section .text

.global _start
_start:
	# Set up end of the stack frame linked list.
	movq $0, %rbp
	pushq %rbp # rip=0
	pushq %rbp # rbp=0
	movq %rsp, %rbp

	# Save argc, argv, environ
	pushq %rdx
	pushq %rsi
	pushq %rdi

	# Prepare malloc, environment
	movq %rdx, %rdi
	call _init_libc

	# Call global constructos
	call _init

	# Restore argc, argv, environ
	popq %rdi
	popq %rsi
	popq %rdx

	# Run main
	call main

	# Cleanly exit the process
	movl %eax, %edi
	call exit

.size _start, . - _start
