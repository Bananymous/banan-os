# uint64_t read_ip()
.global read_ip
read_ip:
	popq %rax
	jmp *%rax

# void start_thread()
.global start_kernel_thread
start_kernel_thread:
	# STACK LAYOUT
	#   on_exit arg
	#   on_exit func
	#   entry arg
	#   entry func

	movq 8(%rsp), %rdi
	movq 0(%rsp), %rsi
	call *%rsi

	movq 24(%rsp), %rdi
	movq 16(%rsp), %rsi
	call *%rsi
