// int setjmp(jmp_buf env)
.global setjmp
setjmp:
	movl 4(%esp), %edx

	leal 4(%esp), %eax
	movl %eax, 0(%edx)

	movl (%esp), %eax
	movl %eax, 4(%edx)

	xorl %eax, %eax

	ret

// void longjmp(jmp_buf env, int val)
.global longjmp
longjmp:
	movl 4(%esp), %edx

	movl 8(%esp), %ecx
	movl $1, %eax
	testl %ecx, %ecx
	cmovnzl %ecx, %eax

	movl 0(%edx), %esp
	movl 4(%edx), %ecx
	jmp *%ecx
