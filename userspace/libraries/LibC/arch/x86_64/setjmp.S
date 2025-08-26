// jmp_buf: rsp, rip, rbx, rbp, r12-r15

// int setjmp(jmp_buf env)
.global setjmp
setjmp:
	leaq 8(%rsp), %rax
	movq %rax, 0(%rdi)

	movq (%rsp), %rax
	movq %rax, 8(%rdi)

	movq %rbx, 16(%rdi)
	movq %rbp, 24(%rdi)
	movq %r12, 32(%rdi)
	movq %r13, 40(%rdi)
	movq %r14, 48(%rdi)
	movq %r15, 56(%rdi)

	xorq %rax, %rax

	ret
 .size setjmp, . - setjmp

// void longjmp(jmp_buf env, int val)
.global longjmp
longjmp:
	movq $1, %rax
	testq %rsi, %rsi
	cmovnzq %rsi, %rax

	movq  0(%rdi), %rsp
	movq  8(%rdi), %rcx
	movq 16(%rdi), %rbx
	movq 24(%rdi), %rbp
	movq 32(%rdi), %r12
	movq 40(%rdi), %r13
	movq 48(%rdi), %r14
	movq 56(%rdi), %r15
	jmp *%rcx
 .size longjmp, . - longjmp

// int sigsetjmp(sigjmp_buf env, int savemask)
.global sigsetjmp
sigsetjmp:
	movq %rsi, 64(%rdi)
	testq %rsi, %rsi
	jz setjmp

	subq $8, %rsp
	movq %rdi, (%rsp)
	leaq 72(%rdi), %rdx
	xorq %rsi, %rsi
	call pthread_sigmask
	movq (%rsp), %rdi
	addq $8, %rsp

	jmp setjmp
 .size sigsetjmp, . - sigsetjmp
