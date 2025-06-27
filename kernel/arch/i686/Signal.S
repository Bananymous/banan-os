.section .userspace, "ax"

// stack contains
//    return address
//    signal number
//    signal handler

.global signal_trampoline
signal_trampoline:
	pusha

	movl 40(%esp), %edi
	movl 36(%esp), %eax

	// align stack to 16 bytes
	movl %esp, %ebp
	andl $-16, %esp

	subl $512, %esp
	fxsave (%esp)

	subl $12, %esp
	pushl %edi
	call *%eax
	addl $16, %esp

	fxrstor (%esp)
	addl $512, %esp

	// restore stack
	movl %ebp, %esp
	popa

	addl $8, %esp
	popf

	ret
