.macro push_userspace
	pushw %gs
	pushw %fs
	pushw %es
	pushw %ds
	pushal
.endm

.macro load_kernel_segments
	movw $0x10, %ax
	movw %ax, %ds
	movw %ax, %es
	movw %ax, %fs

	movw $0x28, %ax
	movw %ax, %gs
.endm

.macro pop_userspace
	popal
	popw %ds
	popw %es
	popw %fs
	popw %gs
.endm

isr_stub:
	push_userspace
	load_kernel_segments

	movl %cr0, %eax; pushl %eax
	movl %cr2, %eax; pushl %eax
	movl %cr3, %eax; pushl %eax
	movl %cr4, %eax; pushl %eax

	movl %esp,     %eax // register ptr
	leal 64(%esp), %ebx // interrupt stack ptr
	movl 60(%esp), %ecx // error code
	movl 56(%esp), %edx // isr number

	movl %esp, %ebp
	subl $15, %esp
	andl $0xFFFFFFF0, %esp

	pushl %eax
	pushl %ebx
	pushl %ecx
	pushl %edx
	call cpp_isr_handler

	movl %ebp, %esp
	addl $16, %esp

	pop_userspace
	addl $8, %esp
	iret

irq_stub:
	push_userspace
	load_kernel_segments

	movl 40(%esp), %eax # interrupt number

	movl %esp, %ebp
	subl $15, %esp
	andl $0xFFFFFFF0, %esp

	subl $12, %esp
	pushl %eax
	call cpp_irq_handler

	movl %ebp, %esp

	pop_userspace
	addl $8, %esp
	iret

.global asm_yield_handler
asm_yield_handler:
	# This can only be called from kernel, so no segment saving is needed
	pushal

	movl %esp,     %eax # interrupt registers ptr
	leal 32(%esp), %ebx # interrupt stack ptr

	movl %esp, %ebp
	subl $15, %esp
	andl $0xFFFFFFF0, %esp

	subl $8, %esp
	pushl %eax
	pushl %ebx
	call cpp_yield_handler

	movl %ebp, %esp

	popal
	iret

.global asm_ipi_handler
asm_ipi_handler:
	push_userspace
	load_kernel_segments

	movl %esp, %ebp
	subl $15, %esp
	andl $0xFFFFFFF0, %esp

	call cpp_ipi_handler

	movl %ebp, %esp

	pop_userspace
	iret


.global asm_timer_handler
asm_timer_handler:
	push_userspace
	load_kernel_segments

	movl %esp, %ebp
	subl $15, %esp
	andl $0xFFFFFFF0, %esp

	call cpp_timer_handler

	movl %ebp, %esp

	pop_userspace
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
