# uint32_t read_ip()
.global read_ip
read_ip:
	popl %eax
	jmp *%eax

# void start_thread(uint32_t sp, uint32_t ip)
.global start_thread
start_thread:
	ud2

# void continue_thread(uint32_t sp, uint32_t ip)
.global continue_thread
continue_thread:
	ud2

# void thread_userspace_trampoline(uint32_t sp, uint32_t ip, int argc, char** argv, char** envp)
.global thread_userspace_trampoline
thread_userspace_trampoline:
	ud2
