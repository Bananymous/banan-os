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


#if defined(__SSE2__)

.global memchr
memchr:
	testq %rdx, %rdx
	jz .memchr_no_match

	movd %esi, %xmm0
	punpcklbw %xmm0, %xmm0
	punpcklwd %xmm0, %xmm0
	pshufd $0, %xmm0, %xmm0

	movq %rdi, %rcx
	andq $15, %rcx
	jz .memchr_loop

	movq %rdi, %rsi
	subq %rcx, %rsi
	movdqa (%rsi), %xmm1
	pcmpeqb %xmm0, %xmm1
	pmovmskb %xmm1, %eax
	shrl %cl, %eax
	jnz .memchr_match

	leaq 16(%rsi), %rdi

	addq %rcx, %rdx
	subq $16, %rdx
	jbe .memchr_no_match

 .memchr_loop:
	movdqa (%rdi), %xmm1
	pcmpeqb %xmm0, %xmm1
	pmovmskb %xmm1, %eax
	testl %eax, %eax
	jnz .memchr_match

	addq $16, %rdi
	subq $16, %rdx
	ja .memchr_loop

 .memchr_no_match:
	xorq %rax, %rax
	ret

 .memchr_match:
	bsfl %eax, %eax
	cmpq %rdx, %rax
	jae .memchr_no_match
	addq %rdi, %rax
	ret

.global memcmp
memcmp:
	testq %rdx, %rdx
	jz .memcmp_equal

	movq %rdi, %rax
	movq %rsi, %rcx
	andq $15, %rax
	andq $15, %rcx
	cmpq %rax, %rcx
	cmovaq %rcx, %rax

	testq %rax, %rax
	jz .memcmp_loop

	movq $16, %rcx
	subq %rax, %rcx

	cmpq %rcx, %rdx
	cmovbq %rdx, %rcx

	subq %rcx, %rdx

 .memcmp_align_loop:
	movzbl (%rdi), %eax
	movzbl (%rsi), %r8d
	subl %r8d, %eax
	jnz .memcmp_return

	incq %rdi
	incq %rsi
	decq %rcx
	jnz .memcmp_align_loop

 .memcmp_loop:
	movdqu (%rdi), %xmm0
	movdqu (%rsi), %xmm1
	pcmpeqb %xmm0, %xmm1
	pmovmskb %xmm1, %eax
	xorl $0xFFFF, %eax
	jnz .memcmp_differ

	addq $16, %rdi
	addq $16, %rsi
	subq $16, %rdx
	ja .memcmp_loop

 .memcmp_equal:
	xorl %eax, %eax
 .memcmp_return:
	ret

 .memcmp_differ:
	bsfl %eax, %ecx
	cmpq %rdx, %rcx
	jae .memcmp_equal
	movzbl (%rdi, %rcx), %eax
	movzbl (%rsi, %rcx), %edx
	subl %edx, %eax
	ret

.global strlen
strlen:
	movq %rdi, %rsi

	pxor %xmm0, %xmm0

	movq %rsi, %rcx
	andq $15, %rcx
	jz .strlen_loop

	movq %rsi, %rdx
	subq %rcx, %rdx
	movdqa (%rdx), %xmm1
	pcmpeqb %xmm0, %xmm1
	pmovmskb %xmm1, %eax
	shrl %cl, %eax
	jnz .strlen_null_found

	leaq 16(%rdx), %rsi

 .strlen_loop:
	movdqa (%rsi), %xmm1
	pcmpeqb %xmm0, %xmm1
	pmovmskb %xmm1, %eax
	testl %eax, %eax
	jnz .strlen_null_found

	addq $16, %rsi
	jmp .strlen_loop

 .strlen_null_found:
	bsfl %eax, %eax
	addq %rsi, %rax
	subq %rdi, %rax
	ret

#endif
