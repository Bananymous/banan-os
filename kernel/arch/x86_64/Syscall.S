// arguments in RAX, RBX, RCX, RDX, RSI, RDI
// System V ABI: RDI, RSI, RDX, RCX, R8, R9
.global asm_syscall_handler
asm_syscall_handler:
	pushq %rbx
	pushq %rcx
	pushq %rdx
	pushq %rdi
	pushq %rsi
	pushq %rbp
	pushq %r8
	pushq %r9
	pushq %r10
	pushq %r11
	pushq %r12
	pushq %r13
	pushq %r14
	pushq %r15

	movq %rsi, %r8
	movq %rdi, %r9
	movq %rax, %rdi
	movq %rbx, %rsi
	xchgq %rcx, %rdx
	leaq 112(%rsp), %rbx
	pushq %rbx
	call cpp_syscall_handler
	addq $8, %rsp

	popq %r15
	popq %r14
	popq %r13
	popq %r12
	popq %r11
	popq %r10
	popq %r9
	popq %r8
	popq %rbp
	popq %rsi
	popq %rdi
	popq %rdx
	popq %rcx
	popq %rbx
	iretq


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
	je .reload_stack

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

 .reload_stack:
	call get_thread_start_sp
	movq %rax, %rsp
	xorq %rax, %rax
	jmp .done
