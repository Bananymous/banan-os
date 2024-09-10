// int setjmp(jmp_buf env)
.global setjmp
setjmp:
	leaq 8(%rsp), %rax
	movq %rax, 0(%rdi)

	movq (%rsp), %rax
	movq %rax, 8(%rdi)

	xorq %rax, %rax

	ret
 .size setjmp, . - setjmp

// void longjmp(jmp_buf env, int val)
.global longjmp
longjmp:
	movq $1, %rax
	testq %rsi, %rsi
	cmovnzq %rsi, %rax

	movq 0(%rdi), %rsp
	movq 8(%rdi), %rcx
	jmp *%rcx
 .size longjmp, . - longjmp
