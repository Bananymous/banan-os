# uint64_t read_ip()
.global read_ip
read_ip:
	popq %rax
	jmp *%rax

exit_thread_trampoline:
	movq 8(%rsp), %rdi
	ret

# void start_thread(uint64_t sp, uint64_t ip)
.global start_thread
start_thread:
	movq %rdi, %rsp
	popq %rdi
	movq $0, %rbp
	pushq $exit_thread_trampoline
	sti
	jmp *%rsi

# void continue_thread(uint64_t sp, uint64_t ip)
.global continue_thread
continue_thread:
	movq %rdi, %rsp
	movq $0, %rax
	jmp *%rsi

# void thread_userspace_trampoline(uint64_t sp, uint64_t ip, int argc, char** argv, char** envp)
.global thread_userspace_trampoline
thread_userspace_trampoline:
	pushq $0x23
	pushq %rdi
	pushfq
	pushq $0x1B
	pushq %rsi
	movq %rdx, %rdi
	movq %rcx, %rsi
	movq %r8, %rdx
	iretq
