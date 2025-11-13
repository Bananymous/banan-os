.section .userspace, "ax"

// stack contains
//    return address
//    return stack
//    return rflags
//    siginfo_t
//    signal number
//    signal handler

.global signal_trampoline
signal_trampoline:
	pushl %esi // gregs
	pushl %edi
	pushl %edx
	pushl %ecx
	pushl %ebx
	pushl %eax
	pushl %ebp

	movl 76(%esp), %eax // return sp
	addl $4, %eax       //   return address
	movl 80(%esp), %ebx // return ip
	pushl %eax;
	pushl %ebx

	// FIXME: populate these
	xorl %eax, %eax
	pushl %eax // stack
	pushl %eax
	pushl %eax
	pushl %eax // sigset
	pushl %eax
	pushl %eax // link

	movl    %esp,  %edx // ucontext
	leal 68(%esp), %esi // siginfo
	movl 64(%esp), %edi // signal number
	movl 60(%esp), %eax // handlers

	// align stack to 16 bytes
	movl %esp, %ebp
	andl $-16, %esp

	subl $512, %esp
	fxsave (%esp)

	subl $4, %esp
	pushl %edx
	pushl %esi
	pushl %edi
	call *%eax
	addl $16, %esp

	fxrstor (%esp)
	addl $512, %esp

	// restore stack
	movl %ebp, %esp
	addl $32, %esp

	// restore registers
	popl %ebp
	popl %eax
	popl %ebx
	popl %ecx
	popl %edx
	popl %edi
	popl %esi

	// skip handler, number, siginfo_t
	addl $44, %esp

	// restore flags
	popf

	movl (%esp), %esp

	ret
