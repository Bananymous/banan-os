# uint32_t read_rip()
.global read_rip
read_rip:
	popl %eax
	jmp *%eax

exit_thread_trampoline:
	addl $16, %esp
	popl %eax
	pushl $0x696969
	pushl %eax
	ret

# void start_thread(uint32_t arg0, uint32_t arg1, uint32_t arg2, uint32_t arg3, uint32_t rsp, uint32_t rbp, uint32_t rip)
.global start_thread
start_thread:
	movl %esp, %eax
	movl 28(%eax), %ecx
	movl 24(%eax), %ebp
	movl 20(%eax), %esp

	pushl 16(%eax)
	pushl 12(%eax)
	pushl 8(%eax)
	pushl 4(%eax)
	pushl $exit_thread_trampoline

	sti
	jmp *%ecx

# void continue_thread(uint32_t rsp, uint32_t rbp, uint32_t rip)
.global continue_thread
continue_thread:
	movl 12(%esp), %ecx
	movl 8(%esp), %ebp
	movl 4(%esp), %esp
	movl $0, %eax
	jmp *%ecx