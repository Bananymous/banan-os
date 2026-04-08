.section .userspace, "ax"

// stack contains
//    (8 bytes)  return address (on return stack)
//    (8 bytes)  return stack
//    (8 bytes)  return rflags
//    (8 bytes)  restore sigmask
//    (56 bytes) siginfo_t
//    (8 bytes)  signal number
//    (8 bytes)  signal handler

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

	movq 208(%rsp), %rax
	pushq %rax; addq $(128 + 8), (%rsp)
	pushq (%rax)

	// FIXME: populate these
	xorq %rax, %rax
	pushq %rax // stack
	pushq %rax
	pushq %rax
	pushq %rax // sigset
	pushq %rax // link

	movq  %rsp,     %rdx // ucontext
	leaq 192(%rsp), %rsi // siginfo
	movq 184(%rsp), %rdi // signal number
	movq 176(%rsp), %rax // handler

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

	// restore sigmask
	movq $83, %rdi       // SYS_SIGPROCMASK
	movq $3,  %rsi       // SIG_SETMASK
	leaq 192(%rsp), %rdx // set
	xorq %r10, %r10      // oset
	syscall

	// restore registers
	addq $16, %rsp
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

	// skip handler, number, siginfo_t, sigmask
	addq $80, %rsp

	// restore flags
	popfq

	movq (%rsp), %rsp

	// return over red-zone
	ret $128
