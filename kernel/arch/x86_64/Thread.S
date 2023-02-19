# uint64_t read_rip()
.global read_rip
read_rip:
	popq %rax
	jmp *%rax

exit_thread_trampoline:
	movq 8(%rsp), %rdi
	ret

# void start_thread(uint64_t arg0, uint64_t arg1, uint64_t arg2, uint64_t arg3, uint64_t rsp, uint64_t rip)
.global start_thread
start_thread:
	movq %r8, %rsp
	movq $0, %rbp
	pushq $exit_thread_trampoline
	sti
	jmp *%r9

# void continue_thread(uint64_t rsp, uint64_t rip)
.global continue_thread
continue_thread:
	movq %rdi, %rsp
	movq $0, %rax
	jmp *%rsi