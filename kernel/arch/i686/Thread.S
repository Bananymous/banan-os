# uint32_t read_ip()
.global read_ip
read_ip:
	popl %eax
	jmp *%eax

# void start_thread(uint32_t sp, uint32_t ip)
.global start_thread
start_thread:
	movl 8(%esp), %edi # ip
	movl 4(%esp), %esp # sp

	# STACK LAYOUT
	#   NULL
	#   thread ptr
	#   &Thread::on_exit
	#   data

	xorl %ebp, %ebp

	sti
	call *%edi

	movl 4(%esp), %edi # &Thread::on_exit

	movl 8(%esp), %eax # thread ptr
	movl %eax, (%esp)

	call *%edi


# void continue_thread(uint32_t sp, uint32_t ip)
.global continue_thread
continue_thread:
	movl 8(%esp), %edi # ip
	movl 4(%esp), %esp # sp
	xorl %eax, %eax
	jmp *%edi

# void thread_userspace_trampoline(uint32_t sp, uint32_t ip, int argc, char** argv, char** envp)
.global thread_userspace_trampoline
thread_userspace_trampoline:
	ud2
