.section .userspace, "ax"

// stack contains
//    return address
//    return stack
//    return rflags
//    siginfo_t
//    signal number
//    signal handler

.global signal_trampoline
signal_trampoline:
	pushq %r15 // gregs
	pushq %r14
	pushq %r13
	pushq %r12
	pushq %r11
	pushq %r10
	pushq %r9
	pushq %r8
	pushq %rsi
	pushq %rdi
	pushq %rdx
	pushq %rcx
	pushq %rbx
	pushq %rax
	pushq %rbp

	// FIXME: populate these
	xorq %rax, %rax
	pushq %rax // stack
	pushq %rax
	pushq %rax
	pushq %rax // sigset
	pushq %rax // link

	movq  %rsp,     %rdx // ucontext
	leaq 176(%rsp), %rsi // siginfo
	movq 168(%rsp), %rdi // signal number
	movq 160(%rsp), %rax // handler

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
	addq $40, %rsp

	// restore registers
	popq %rbp
	popq %rax
	popq %rbx
	popq %rcx
	popq %rdx
	popq %rdi
	popq %rsi
	popq %r8
	popq %r9
	popq %r10
	popq %r11
	popq %r12
	popq %r13
	popq %r14
	popq %r15

	// skip handler, number, siginfo_t
	addq $72, %rsp

	// restore flags
	popfq

	movq (%rsp), %rsp

	// return over red-zone and siginfo_t
	ret $128
