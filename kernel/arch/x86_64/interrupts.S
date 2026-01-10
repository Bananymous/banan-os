.macro swapgs_if_necessary, n
	testb $3, \n(%rsp)
	jz 1f; jnp 1f
	swapgs
1:
.endm

.macro pushaq, n
	swapgs_if_necessary \n
	pushq %rax
	pushq %rcx
	pushq %rdx
	pushq %rbx
	pushq %rbp
	pushq %rsi
	pushq %rdi
	pushq %r8
	pushq %r9
	pushq %r10
	pushq %r11
	pushq %r12
	pushq %r13
	pushq %r14
	pushq %r15
.endm

.macro popaq, n
	popq %r15
	popq %r14
	popq %r13
	popq %r12
	popq %r11
	popq %r10
	popq %r9
	popq %r8
	popq %rdi
	popq %rsi
	popq %rbp
	popq %rbx
	popq %rdx
	popq %rcx
	popq %rax
	swapgs_if_necessary \n
.endm

isr_stub:
	pushaq 24
	cld
	movq %cr0, %rax; pushq %rax
	movq %cr2, %rax; pushq %rax
	movq %cr3, %rax; pushq %rax
	movq %cr4, %rax; pushq %rax

	movq 152(%rsp), %rdi	// isr number
	movq 160(%rsp), %rsi	// error code
	leaq 168(%rsp), %rdx	// interrupt stack ptr
	movq %rsp, %rcx			// register ptr
	call cpp_isr_handler
	addq $32, %rsp

	popaq 24
	addq $16, %rsp
	iretq

irq_stub:
	pushaq 24
	cld
	movq 120(%rsp), %rdi	# irq number
	call cpp_irq_handler
	popaq 24
	addq $16, %rsp
	iretq

.global asm_ipi_handler
asm_ipi_handler:
	pushaq 8
	cld
	call cpp_ipi_handler
	popaq 8
	iretq

.global asm_timer_handler
asm_timer_handler:
	pushaq 8
	cld
	call cpp_timer_handler
	popaq 8
	iretq

.macro isr n
	.global isr\n
	isr\n:
		pushq $0
		pushq $\n
		jmp isr_stub
.endm

.macro isr_err n
	.global isr\n
	isr\n:
		pushq $\n
		jmp isr_stub
.endm

.macro irq n
	.global irq\n
	irq\n:
		pushq $0
		pushq $\n
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
