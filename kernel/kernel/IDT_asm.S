.globl handle_irq_common

handle_irq_common:
	pusha
	pushw %ds
	pushw %es
	pushw %ss
	pushw %ss
	popw %ds
	popw %es
	call handle_irq
	popw %es
	popw %ds
	popa
	iret