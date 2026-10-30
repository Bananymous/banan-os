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

	# load kernel segments
	movw $0x10, %bp
	movw %bp, %ds
	movw %bp, %es
	movw %bp, %fs
	movw $0x28, %bp
	movw %bp, %gs

	# align stack
	movl %esp, %ebp
	andl $-16, %esp

	# handle fork
	cmpl $8, %eax
	je .Lasm_syscall_handler_fork

	subl $8, %esp
	pushl %edi
	pushl %esi
	pushl %edx
	pushl %ecx
	pushl %ebx
	pushl %eax
	call cpp_syscall_handler

	movl %ebp, %esp

 .Lasm_syscall_handler_done:
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
	call read_eip
	testl %eax, %eax
	jz .Lasm_syscall_handler_done

	subl $4, %esp
	pushl %eax
	pushl %ebp
	pushl $8
	call cpp_syscall_handler

	movl %ebp, %esp
	jmp .Lasm_syscall_handler_done
