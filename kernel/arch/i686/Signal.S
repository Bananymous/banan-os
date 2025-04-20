.section .userspace, "ax"

// stack contains
//    return address
//    signal number
//    signal handler

.global signal_trampoline
signal_trampoline:
	pushl %ebp
	movl %esp, %ebp

	pusha

	movl 40(%esp), %edi
	movl 36(%esp), %eax

	// align stack to 16 bytes
	movl %esp, %ebp
	andl $-16, %esp

	subl $12, %esp
	pushl %edi
	call *%eax
	addl $16, %esp

	// restore stack
	movl %ebp, %esp
	popa

	leave
	addl $8, %esp

	ret
