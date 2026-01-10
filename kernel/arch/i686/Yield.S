.global asm_yield_trampoline
asm_yield_trampoline:
	movl %esp, %ecx
	movl 4(%esp), %esp

	pushl (%ecx)
	pushl %ecx
	pushl %eax
	pushl %ebx
	pushl %esi
	pushl %edi
	pushl %ebp

	pushl %esp
	call scheduler_on_yield
	addl $4, %esp

	popl %ebp
	popl %edi
	popl %esi
	popl %ebx
	popl %eax
	movl 4(%esp), %ecx
	movl 0(%esp), %esp
	jmp *%ecx
