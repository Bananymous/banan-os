isr_stub:
	pusha

	movl %cr0, %eax; pushl %eax
	movl %cr2, %eax; pushl %eax
	movl %cr3, %eax; pushl %eax
	movl %cr4, %eax; pushl %eax

	movl %esp,     %eax // register ptr
	leal 56(%esp), %ebx // interrupt stack ptr
	movl 52(%esp), %ecx // error code
	movl 48(%esp), %edx // isr number

	subl $12, %esp
	pushl %eax
	pushl %ebx
	pushl %ecx
	pushl %edx
	call cpp_isr_handler
	addl $44, %esp

	popa
	addl $8, %esp
	iret

irq_stub:
	pusha

	leal 40(%esp), %eax // interrupt stack ptr
	movl 32(%esp), %ebx // irq number

	subl $12, %esp
	pushl %eax
	pushl %ebx
	call cpp_irq_handler
	addl $20, %esp

	popa
	addl $8, %esp
	iret

// arguments in EAX, EBX, ECX, EDX, ESI, EDI
.global syscall_asm
syscall_asm:
	ud2
	pusha

	pushl %esp
	addl $36, (%esp)

	pushl %edi
	pushl %esi
	pushl %edx
	pushl %ecx
	pushl %ebx
	pushl %eax

	call cpp_syscall_handler

	addl $60, %esp

	popl %edi
	popl %esi
	popl %ebp
	addl $4, %esp
	popl %ebx
	popl %edx
	popl %ecx
	addl $4, %esp

	iret

.macro isr n
	.global isr\n
	isr\n:
		pushl $0
		pushl $\n
		jmp isr_stub
.endm

.macro isr_err n
	.global isr\n
	isr\n:
		pushl $\n
		jmp isr_stub
.endm

.macro irq n
	.global irq\n
	irq\n:
		pushl $0
		pushl $\n
		jmp irq_stub
.endm

isr 0
isr 1
isr 2
isr 3
isr 4
isr 5
isr 6
isr 7
isr_err 8
isr 9
isr_err 10
isr_err 11
isr_err 12
isr_err 13
isr_err 14
isr 15
isr 16
isr_err 17
isr 18
isr 19
isr 20
isr 21
isr 22
isr 23
isr 24
isr 25
isr 26
isr 27
isr 28
isr 29
isr 30
isr 31

irq 0
irq 1
irq 2
irq 3
irq 4
irq 5
irq 6
irq 7
irq 8
irq 9
irq 10
irq 11
irq 12
irq 13
irq 14
irq 15
irq 16
irq 17
irq 18
irq 19
irq 20
irq 21
irq 22
irq 23
irq 24
irq 25
irq 26
irq 27
irq 28
irq 29
irq 30
irq 31
irq 32
