.section .userspace, "ax"

// stack contains
//    return address
//    signal number
//    signal handler

.global signal_trampoline
signal_trampoline:
	pushq %rax
	pushq %rbx
	pushq %rcx
	pushq %rdx
	pushq %rbp
	pushq %rdi
	pushq %rsi
	pushq %r8
	pushq %r9
	pushq %r10
	pushq %r11
	pushq %r12
	pushq %r13
	pushq %r14
	pushq %r15

	movq 128(%rsp), %rdi
	movq 120(%rsp), %rax

	// align stack to 16 bytes
	movq %rsp, %rbp
	andq $-16, %rsp

	subq $512, %rsp
	fxsave64 (%rsp)

	call *%rax

	fxrstor64 (%rsp)
	addq $512, %rsp

	// restore stack
	movq %rbp, %rsp
	popq %r15
	popq %r14
	popq %r13
	popq %r12
	popq %r11
	popq %r10
	popq %r9
	popq %r8
	popq %rsi
	popq %rdi
	popq %rbp
	popq %rdx
	popq %rcx
	popq %rbx
	popq %rax

	addq $16, %rsp
	popfq

	// return over red-zone
	ret $128
