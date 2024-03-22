.global sys_fork_trampoline
sys_fork_trampoline:
	subl $4, %esp
	pushl %ebx
	pushl %ebp
	call read_rip
	testl %eax, %eax
	je .done
	subl $8, %esp
	pushl %eax
	pushl %esp
	call sys_fork
	addl $16, %esp
.done:
	popl %ebp
	popl %ebx
	addl $4, %esp
	ret
