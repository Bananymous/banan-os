# uint64_t read_rip()
.global read_rip
read_rip:
	popq %rax
	jmp *%rax

exit_thread_trampoline:
	movq 8(%rsp), %rdi
	ret

# void start_thread(uint64_t rsp, uint64_t rip)
.global start_thread
start_thread:
	movq %rdi, %rsp
	popq %rdi
	movq $0, %rbp
	pushq $exit_thread_trampoline
	sti
	jmp *%rsi

# void continue_thread(uint64_t rsp, uint64_t rip)
.global continue_thread
continue_thread:
	movq %rdi, %rsp
	movq $0, %rax
	jmp *%rsi