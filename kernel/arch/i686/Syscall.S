// arguments in EAX, EBX, ECX, EDX, ESI, EDI
.global asm_syscall_handler
asm_syscall_handler:
	# save segment registers
	pushw %ds
	pushw %es
	pushw %fs
	pushw %gs

	# save general purpose registers
	pushl %ebx
	pushl %ecx
	pushl %edx
	pushl %edi
	pushl %esi
	pushl %ebp
	cld

	# align stack
	movl %esp, %ebp
	subl $15, %esp
	andl $0xFFFFFFF0, %esp

	# push arguments
	subl $4, %esp
	pushl %ebp
	addl $32, (%esp)
	pushl %edi
	pushl %esi
	pushl %edx
	pushl %ecx
	pushl %ebx
	pushl %eax

	# load kernel segments
	movw $0x10, %ax
	movw %ax, %ds
	movw %ax, %es
	movw %ax, %fs
	movw $0x28, %ax
	movw %ax, %gs

	call cpp_syscall_handler

	movl %ebp, %esp

	# restore general purpose registers
	popl %ebp
	popl %esi
	popl %edi
	popl %edx
	popl %ecx
	popl %ebx

	# restore segment registers
	popw %gs
	popw %fs
	popw %es
	popw %ds

	iret

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
