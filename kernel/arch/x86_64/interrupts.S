.macro pushaq
	pushq %rax
	pushq %rbx
	pushq %rcx
	pushq %rdx
	pushq %rbp
	pushq %rdi
	pushq %rsi
	pushq %r8
	pushq %r9
	pushq %r10
	pushq %r11
	pushq %r12
	pushq %r13
	pushq %r14
	pushq %r15
.endm

.macro popaq
	popq %r15
	popq %r14
	popq %r13
	popq %r12
	popq %r11
	popq %r10
	popq %r9
	popq %r8
	popq %rsi
	popq %rdi
	popq %rbp
	popq %rdx
	popq %rcx
	popq %rbx
	popq %rax
.endm

.macro popaq_no_rax
	popq %r15
	popq %r14
	popq %r13
	popq %r12
	popq %r11
	popq %r10
	popq %r9
	popq %r8
	popq %rsi
	popq %rdi
	popq %rbp
	popq %rdx
	popq %rcx
	popq %rbx
.endm

isr_stub:
	pushaq

	movq     %cr0,  %rax; pushq %rax
	movq     %cr2,  %rax; pushq %rax
	movq     %cr3,  %rax; pushq %rax
	movq     %cr4,  %rax; pushq %rax
	movq 184(%rsp), %rax; pushq %rax
	movq 176(%rsp), %rax; pushq %rax
	movq 208(%rsp), %rax; pushq %rax

	movq 176(%rsp), %rdi
	movq 184(%rsp), %rsi
	movq %rsp, %rdx

	call cpp_isr_handler
	addq $56, %rsp
	popaq
	addq $16, %rsp
	iretq

irq_stub:
	pushaq
	movq 0x78(%rsp), %rdi	# irq number
	call cpp_irq_handler
	popaq
	addq $16, %rsp
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

// arguments in RAX, RBX, RCX, RDX, RSI, RDI
// System V ABI: RDI, RSI, RDX, RCX, R8, R9
.global syscall_asm
syscall_asm:
	pushaq
	movq %rsi, %r8
	movq %rdi, %r9
	movq %rax, %rdi
	movq %rbx, %rsi
	xchgq %rcx, %rdx
	movq %rsp, %rbx
	addq $120, %rbx
	pushq %rbx
	call cpp_syscall_handler
	addq $8, %rsp
	popaq_no_rax
	addq $8, %rsp
	iretq