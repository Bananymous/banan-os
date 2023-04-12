# uint32_t read_rip()
.global read_rip
read_rip:
	popl %eax
	jmp *%eax

exit_thread_trampoline:
	addl $4, %esp
	pushl (%esp)
	ret

# void start_thread(uint32_t esp, uint32_t eip)
.global start_thread
start_thread:
	movl 8(%esp), %ecx
	movl 4(%esp), %esp
	movl $0, %ebp
	pushl $exit_thread_trampoline
	sti
	jmp *%ecx

# void continue_thread(uint32_t rsp, uint32_t rip)
.global continue_thread
continue_thread:
	movl 8(%esp), %ecx
	movl 4(%esp), %esp
	movl $0, %eax
	jmp *%ecx

# void thread_jump_userspace(uint32_t rsp, uint32_t rip)
.global thread_jump_userspace
thread_jump_userspace:
	movl $0x23, %eax
	movw %ax, %ds
	movw %ax, %es
	movw %ax, %fs
	movw %ax, %gs

	movl 8(%esp), %ecx
	movl 4(%esp), %esp

	pushl $0x23
	pushl %esp
	pushfl
	pushl $0x1B
	pushl %ecx
	iret
