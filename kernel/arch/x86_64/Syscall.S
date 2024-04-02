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
