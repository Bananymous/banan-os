.section .userspace, "aw"

// stack contains
//    return address
//    signal number
//    signal handler

.global signal_trampoline
signal_trampoline:
	ud2

	pushl %ebp
	movl %esp, %ebp
	subl $8, %esp

	pusha

	movl 40(%esp), %edi
	movl 36(%esp), %eax

	subl $12, %esp
	pushl %edi
	call *%eax
	addl $16, %esp

	popa

	leave
	addl $8, %esp

	ret
