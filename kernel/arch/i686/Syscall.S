read_eip:
	popl %eax
	jmp *%eax

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

	# handle fork
	cmpl $8, %eax
	je .Lasm_syscall_handler_fork

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

 .Lasm_syscall_handler_done:
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

 .Lasm_syscall_handler_fork:
	pushl %ebp
	pushl %ebx
	pushl %esi
	pushl %edi

	call read_eip
	testl %eax, %eax
	jz .Lasm_syscall_handler_fork_child

	movl %esp, %ebx

	subl $4, %esp
	pushl %eax
	pushl %ebx
	pushl $8
	call cpp_syscall_handler

	addl $32, %esp
	jmp .Lasm_syscall_handler_done

 .Lasm_syscall_handler_fork_child:
	popl %edi
	popl %ebx
	popl %ebx
	popl %ebp
	jmp .Lasm_syscall_handler_done
