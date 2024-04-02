# uint64_t read_ip()
.global read_ip
read_ip:
	popq %rax
	jmp *%rax

# void start_kernel_thread()
.global start_kernel_thread
start_kernel_thread:
	call get_thread_start_sp
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

.global start_userspace_thread
start_userspace_thread:
	call get_thread_start_sp
	movq %rax, %rsp

	# STACK LAYOUT
	#   entry
	#   argc
	#   argv
	#   envp
	#   userspace stack

	call get_userspace_thread_stack_top

	popq %rdx
	popq %rsi
	popq %rdi
	popq %rcx

	pushq $(0x20 | 3)
	pushq %rax
	pushq $0x202
	pushq $(0x18 | 3)
	pushq %rcx
	iretq
