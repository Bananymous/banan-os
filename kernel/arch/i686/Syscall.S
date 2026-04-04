// arguments in EAX, EBX, ECX, EDX, ESI, EDI
.global asm_syscall_handler
asm_syscall_handler:
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
	andl $-16, %esp

	# push arguments
	subl $8, %esp
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

	# restore userspace segments
	movw $(0x20 | 3), %bx
	movw %bx, %ds
	movw %bx, %es
	movw $(0x30 | 3), %bx
	movw %bx, %fs
	movw $(0x38 | 3), %bx
	movw %bx, %gs

	# restore general purpose registers
	popl %ebp
	popl %esi
	popl %edi
	popl %edx
	popl %ecx
	popl %ebx

	iret

.global sys_fork_trampoline
sys_fork_trampoline:
	pushl %ebp
	pushl %ebx
	pushl %esi
	pushl %edi

	call read_ip
	testl %eax, %eax
	jz .done

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
