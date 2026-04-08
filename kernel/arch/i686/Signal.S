.section .userspace, "ax"

// stack contains
//    (4 bytes)  return address (on return stack)
//    (4 bytes)  return stack
//    (4 bytes)  return rflags
//    (8 bytes)  restore sigmask
//    (36 bytes) siginfo_t
//    (4 bytes)  signal number
//    (4 bytes)  signal handler

.global signal_trampoline
signal_trampoline:
	pushl %esi // gregs
	pushl %edi
	pushl %edx
	pushl %ecx
	pushl %ebx
	pushl %eax
	pushl %ebp

	movl 80(%esp), %eax
	pushl %eax; addl $4, (%esp)
	pushl (%eax)

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
	addl $24, %esp

	// restore sigmask
	movl $83, %eax      // SYS_SIGPROCMASK
	movl $3,  %ebx      // SIG_SETMASK
	leal 72(%esp), %ecx // set
	xorl %edx, %edx     // oset
	int $0xF0

	// restore registers
	addl $8, %esp
	popl %ebp
	popl %eax
	popl %ebx
	popl %ecx
	popl %edx
	popl %edi
	popl %esi

	// skip handler, number, siginfo_t, sigmask
	addl $52, %esp

	// restore flags
	popf

	movl (%esp), %esp

	ret
