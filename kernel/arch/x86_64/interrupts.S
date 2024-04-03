.macro pushaq
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

.macro popaq
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
.endm

isr_stub:
	pushaq

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

	popaq
	addq $16, %rsp
	iretq

irq_stub:
	pushaq
	movq 120(%rsp), %rdi	# irq number
	call cpp_irq_handler
	popaq
	addq $16, %rsp
	iretq

.global asm_yield_handler
asm_yield_handler:
	pushaq
	leaq 120(%rsp), %rdi # interrupt stack ptr
	movq     %rsp,  %rsi # interrupt register ptr
	call cpp_yield_handler
	popaq
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
