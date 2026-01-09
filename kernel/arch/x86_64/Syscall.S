.global asm_syscall_handler
asm_syscall_handler:
	swapgs

	movq %rsp, %rax
	movq %gs:8, %rsp

	pushq $(0x20 | 3)
	pushq %rax
	pushq %r11
	pushq $(0x28 | 3)
	pushq %rcx
	subq $8, %rsp

	movq %r10, %rcx
	call cpp_syscall_handler

	movq  8(%rsp), %rcx
	movq 24(%rsp), %r11
	movq 32(%rsp), %rsp

	swapgs
	sysretq

.global sys_fork_trampoline
sys_fork_trampoline:
	pushq %rbx
	pushq %rbp
	pushq %r12
	pushq %r13
	pushq %r14
	pushq %r15

	call read_ip
	testq %rax, %rax
	je .done

	movq %rax, %rsi
	movq %rsp, %rdi
	call sys_fork

 .done:
	popq %r15
	popq %r14
	popq %r13
	popq %r12
	popq %rbp
	popq %rbx
	ret
