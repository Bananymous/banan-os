.global memchr
memchr:
	movb %sil, %al
	movq %rdx, %rcx
	movq $1, %rdx
	cmpq $1, %rcx # clear ZF if count is zero
	repne scasb
	cmoveq %rdi, %rdx
	leaq -1(%rdx), %rax
	ret

.global memcmp
memcmp:
	movq %rdx, %rcx
	testq %rcx, %rcx # set ZF if count is zero
	repe cmpsb
	jne .memcmp_not_equal
	xorq %rax, %rax
	ret
 .memcmp_not_equal:
	movzbl -1(%rdi), %eax
	movzbl -1(%rsi), %ecx
	subq %rcx, %rax
	ret

.global memcpy
memcpy:
	movq %rdi, %rax
	movq %rdx, %rcx
	rep movsb
	ret

.global memmove
memmove:
	cmpq %rdi, %rsi
	jae memcpy
	movq %rdi, %rax
	leaq -1(%rdi, %rdx), %rdi
	leaq -1(%rsi, %rdx), %rsi
	movq %rdx, %rcx
	std
	rep movsb
	cld
	ret

.global memset
memset:
	movq %rdi, %r8
	movb %sil, %al
	movq %rdx, %rcx
	rep stosb
	movq %r8, %rax
	ret

.global strlen
strlen:
	xorb %al, %al
	movq $-1, %rcx
	repne scasb
	movq $-2, %rax
	subq %rcx, %rax
	ret
