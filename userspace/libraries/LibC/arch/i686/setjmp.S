// jmp_buf: esp, eip, ebx, ebp, edi, esi

// int setjmp(jmp_buf env)
.global setjmp
setjmp:
	movl 4(%esp), %edx

	leal 4(%esp), %eax
	movl %eax, 0(%edx)

	movl (%esp), %eax
	movl %eax, 4(%edx)

	movl %ebx,  8(%edx)
	movl %ebp, 12(%edx)
	movl %edi, 16(%edx)
	movl %esi, 20(%edx)

	xorl %eax, %eax

	ret

 .size setjmp, . - setjmp

// void longjmp(jmp_buf env, int val)
.global longjmp
longjmp:
	movl 4(%esp), %edx

	movl 8(%esp), %ecx
	movl $1, %eax
	testl %ecx, %ecx
	cmovnzl %ecx, %eax

	movl  0(%edx), %esp
	movl  4(%edx), %ecx
	movl  8(%edx), %ebx
	movl 12(%edx), %ebp
	movl 16(%edx), %edi
	movl 20(%edx), %esi
	jmp *%ecx

 .size longjmp, . - longjmp
