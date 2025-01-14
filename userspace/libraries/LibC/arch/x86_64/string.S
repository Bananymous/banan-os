.global memchr
memchr:
	movb %sil, %al
	movq %rdx, %rcx
	repne scasb
	xorq %rax, %rax
	testq %rcx, %rcx
	cmovnzq %rdi, %rax
	ret

.global memcmp
memcmp:
	movq %rdx, %rcx
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
	movq %rdi, %rdx
	rep movsb
	movq %rdx, %rax
	ret

.global memmove
memmove:
	cmpq %rdi, %rsi
	jae memcpy
	leaq -1(%rdi, %rdx), %rdi
	leaq -1(%rsi, %rdx), %rsi
	movq %rdx, %rcx
	std
	rep movsb
	cld
	leaq 1(%rdi), %rax
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
