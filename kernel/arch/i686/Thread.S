# uint32_t read_ip()
.global read_ip
read_ip:
	popl %eax
	jmp *%eax

# void start_kernel_thread()
.global start_kernel_thread
start_kernel_thread:
	call get_thread_start_sp
	movl %eax, %esp

	# STACK LAYOUT
	#   on_exit arg
	#   on_exit func
	#   entry arg
	#   entry func

	movl 4(%esp), %edi
	movl 0(%esp), %esi

	subl $12, %esp
	pushl %edi
	sti
	call *%esi
	addl $16, %esp

	movl 12(%esp), %edi
	movl  8(%esp), %esi

	subl $12, %esp
	pushl %edi
	call *%esi
	addl $16, %esp


.global start_userspace_thread
start_userspace_thread:
	call get_thread_start_sp
	movl %eax, %esp

	# STACK LAYOUT
	#   entry
	#   argc
	#   argv
	#   envp
	#   userspace stack

	call get_userspace_thread_stack_top

	movw $(0x20 | 3), %bx
	movw %bx, %ds
	movw %bx, %es
	movw %bx, %fs
	movw %bx, %gs
	xorw %bx, %bx

	popl %edx
	popl %esi
	popl %edi
	popl %ecx

	pushl $(0x20 | 3)
	pushl %eax
	pushl $0x202
	pushl $(0x18 | 3)
	pushl %ecx
	iret
