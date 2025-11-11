.macro maybe_load_kernel_segments, n
	cmpb $0x08, \n(%esp)
	je 1f

	movw $0x10, %ax
	movw %ax, %ds
	movw %ax, %es
	movw %ax, %fs

	movw $0x28, %ax
	movw %ax, %gs
1:
.endm

.macro maybe_load_userspace_segments, n
	cmpb $0x08, \n(%esp)
	je 1f

	movw $(0x20 | 3), %bx
	movw %bx, %ds
	movw %bx, %es
	movw $(0x30 | 3), %bx
	movw %bx, %fs
	movw $(0x38 | 3), %bx
	movw %bx, %gs
1:
.endm

isr_stub:
	pushal
	maybe_load_kernel_segments 44
	cld

	movl %cr0, %eax; pushl %eax
	movl %cr2, %eax; pushl %eax
	movl %cr3, %eax; pushl %eax
	movl %cr4, %eax; pushl %eax

	movl 48(%esp), %edi // isr number
	movl 52(%esp), %esi // error code
	leal 56(%esp), %edx // interrupt stack ptr
	movl %esp,     %ecx // register ptr

	# stack frame for stack trace
	leal 56(%esp), %eax
	pushl (%eax)
	pushl %ebp

	movl %esp, %ebp
	andl $-16, %esp

	pushl %ecx
	pushl %edx
	pushl %esi
	pushl %edi
	call cpp_isr_handler

	movl %ebp, %esp
	addl $24, %esp

	maybe_load_userspace_segments 44
	popal
	addl $8, %esp
	iret

irq_stub:
	pushal
	maybe_load_kernel_segments 44
	cld

	movl 32(%esp), %edi # interrupt number

	movl %esp, %ebp
	andl $-16, %esp

	subl $12, %esp
	pushl %edi
	call cpp_irq_handler

	movl %ebp, %esp

	maybe_load_userspace_segments 44
	popal
	addl $8, %esp
	iret

.global asm_yield_handler
asm_yield_handler:
	# This can only be called from kernel, so no segment saving is needed
	pushal
	cld

	leal 32(%esp), %edi # interrupt stack ptr
	movl %esp,     %esi # interrupt registers ptr

	movl %esp, %ebp
	andl $-16, %esp

	subl $8, %esp
	pushl %esi
	pushl %edi
	call cpp_yield_handler

	movl %ebp, %esp

	popal
	iret

.global asm_ipi_handler
asm_ipi_handler:
	pushal
	maybe_load_kernel_segments 36
	cld

	movl %esp, %ebp
	andl $-16, %esp

	call cpp_ipi_handler

	movl %ebp, %esp

	maybe_load_userspace_segments 36
	popal
	iret

.global asm_timer_handler
asm_timer_handler:
	pushal
	maybe_load_kernel_segments 36
	cld

	movl %esp, %ebp
	andl $-16, %esp

	call cpp_timer_handler

	movl %ebp, %esp

	maybe_load_userspace_segments 36
	popal
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

.irp i,  0,  1,  2,  3,  4,  5,  6,  7,  8,  9, \
        10, 11, 12, 13, 14, 15, 16, 17, 18, 19, \
        20, 21, 22, 23, 24, 25, 26, 27, 28, 29, \
        30, 31, 32, 33, 34, 35, 36, 37, 38, 39, \
        40, 41, 42, 43, 44, 45, 46, 47, 48, 49, \
        50, 51, 52, 53, 54, 55, 56, 57, 58, 59, \
        60, 61, 62, 63, 64, 65, 66, 67, 68, 69, \
        70, 71, 72, 73, 74, 75, 76, 77, 78, 79, \
        80, 81, 82, 83, 84, 85, 86, 87, 88, 89, \
        90, 91, 92, 93, 94, 95, 96, 97, 98, 99, \
       100,101,102,103,104,105,106,107,108,109, \
       110,111,112,113,114,115,116,117,118,119, \
       120,121,122,123,124,125,126,127,128,129, \
       130,131,132,133,134,135,136,137,138,139, \
       140,141,142,143,144,145,146,147,148,149, \
       150,151,152,153,154,155,156,157,158,159, \
       160,161,162,163,164,165,166,167,168,169, \
       170,171,172,173,174,175,176,177,178,179, \
       180,181,182,183,184,185,186,187,188,189, \
       190,191,192,193,194,195,196,197,198,199, \
       200,201,202,203,204,205,206,207
	irq \i
.endr
