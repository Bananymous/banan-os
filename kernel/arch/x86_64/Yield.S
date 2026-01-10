.global asm_yield_trampoline
asm_yield_trampoline:
	movq %rsp, %rcx
	movq %rdi, %rsp

	subq $8, %rsp
	pushq (%rcx)
	pushq %rcx
	pushq %rax
	pushq %rbx
	pushq %rbp
	pushq %r12
	pushq %r13
	pushq %r14
	pushq %r15

	movq %rsp, %rdi
	call scheduler_on_yield

	popq %r15
	popq %r14
	popq %r13
	popq %r12
	popq %rbp
	popq %rbx
	popq %rax
	movq 8(%rsp), %rcx
	movq 0(%rsp), %rsp
	jmp *%rcx
