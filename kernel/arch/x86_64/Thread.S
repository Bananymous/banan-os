# uint64_t read_ip()
.global read_ip
read_ip:
	popq %rax
	jmp *%rax

# void start_kernel_thread()
.global start_kernel_thread
start_kernel_thread:
	call get_start_kernel_thread_sp
	movq %rax, %rsp

	# STACK LAYOUT
	#   on_exit arg
	#   on_exit func
	#   entry arg
	#   entry func

	movq 8(%rsp), %rdi
	movq 0(%rsp), %rsi
	sti
	call *%rsi

	movq 24(%rsp), %rdi
	movq 16(%rsp), %rsi
	call *%rsi
