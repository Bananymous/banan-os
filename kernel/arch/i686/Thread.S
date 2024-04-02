# uint32_t read_ip()
.global read_ip
read_ip:
	popl %eax
	jmp *%eax

# void start_kernel_thread()
.global start_kernel_thread
start_kernel_thread:
	call get_start_kernel_thread_sp
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
