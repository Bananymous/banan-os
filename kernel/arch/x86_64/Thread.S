# uint64_t read_rip()
.global read_rip
read_rip:
	popq %rax
	jmp *%rax

.global get_thread_at_exit
get_thread_at_exit:
	movq 8(%rdi), %rax
	ret

exit_thread_trampoline:
	movq 8(%rsp), %rdi
	ret

# void start_thread(uint64_t arg0, uint64_t arg1, uint64_t arg2, uint64_t arg3, uint64_t rsp, uint64_t rbp, uint64_t rip)
.global start_thread
start_thread:
	movq 8(%rsp), %rcx
	movq %r8, %rsp
	movq %r9, %rbp
	pushq $exit_thread_trampoline
	sti
	jmp *%rcx

# void continue_thread(uint64_t rsp, uint64_t rbp, uint64_t rip)
.global continue_thread
continue_thread:
	movq %rdi, %rsp
	movq %rsi, %rbp
	movq $0, %rax
	jmp *%rdx