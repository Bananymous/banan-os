.global sys_fork_trampoline
sys_fork_trampoline:
	pushl %ebp
	pushl %ebx
	pushl %esi
	pushl %edi

	call read_ip
	testl %eax, %eax
	jz .reload_stack

	movl %esp, %ebx

	subl $8, %esp
	pushl %eax
	pushl %ebx
	call sys_fork
	addl $16, %esp

 .done:
	popl %edi
	popl %esi
	popl %ebx
	popl %ebp
	ret

 .reload_stack:
	call get_thread_start_sp
	movl %eax, %esp
	xorl %eax, %eax
	jmp .done
