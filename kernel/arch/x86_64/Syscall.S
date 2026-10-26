read_rip:
	popq %rax
	jmp *%rax

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

	cmpq $8, %rdi
	je .Lasm_syscall_handler_fork

	movq %r10, %rcx
	call cpp_syscall_handler

 .Lasm_syscall_handler_done:
	movq  8(%rsp), %rcx
	movq 24(%rsp), %r11
	movq 32(%rsp), %rsp

	swapgs
	sysretq

 .Lasm_syscall_handler_fork:
	pushq %rbx
	pushq %rbp
	pushq %r12
	pushq %r13
	pushq %r14
	pushq %r15

	call read_rip
	testq %rax, %rax
	jz .Lasm_syscall_handler_fork_child

	movq %rax, %rsi
	movq %rsp, %rdi
	call cpp_fork_handler

	addq $48, %rsp
	jmp .Lasm_syscall_handler_done

 .Lasm_syscall_handler_fork_child:
	popq %r15
	popq %r14
	popq %r13
	popq %r12
	popq %rbp
	popq %rbx
	jmp .Lasm_syscall_handler_done
