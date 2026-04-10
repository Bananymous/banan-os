.set nt_threshold, 32 * 1024

.align 16
.global memcpy
memcpy:
	cmpq $nt_threshold, %rdx
	jae .Lmemcpy_nt
	movq %rdi, %rax
	movq %rdx, %rcx
	rep movsb
	ret

 .Lmemcpy_nt:
	movq %rdi, %rax

	testq $63, %rdi
	jz .Lmemcpy_nt_aligned

	# NOTE: this unconditionally copies 64 bytes to align to 64 byte boundary
	#       but as nt branch is only taken for huge buffers, it doesnt add much overhead

	movdqu  0(%rsi), %xmm0
	movdqu 16(%rsi), %xmm1
	movdqu 32(%rsi), %xmm2
	movdqu 48(%rsi), %xmm3

	movdqu %xmm0,  0(%rdi)
	movdqu %xmm1, 16(%rdi)
	movdqu %xmm2, 32(%rdi)
	movdqu %xmm3, 48(%rdi)

	movq %rdi, %rcx
	andq $63, %rcx
	leaq -64(%rdx, %rcx), %rdx

	negq %rcx
	leaq 64(%rdi, %rcx), %rdi
	leaq 64(%rsi, %rcx), %rsi

 .Lmemcpy_nt_aligned:
	movq %rdx, %rcx
	shrq $6, %rdx

 .align 16
 .Lmemcpy_nt_loop:
	prefetchnta    256(%rsi)
	prefetchnta 32+256(%rsi)

	movdqu  0(%rsi), %xmm0
	movdqu 16(%rsi), %xmm1
	movdqu 32(%rsi), %xmm2
	movdqu 48(%rsi), %xmm3

	movntdq %xmm0,  0(%rdi)
	movntdq %xmm1, 16(%rdi)
	movntdq %xmm2, 32(%rdi)
	movntdq %xmm3, 48(%rdi)

	addq $64, %rdi
	addq $64, %rsi
	subq $1, %rdx
	jnz .Lmemcpy_nt_loop

	andq $63, %rcx
	rep movsb
	sfence
	ret

.align 16
.global memmove
memmove:
	cmpq $nt_threshold, %rdx
	jb .Lmemmove_small
	leaq (%rdi, %rdx), %rax
	cmpq %rax, %rsi
	jae .Lmemcpy_nt
	leaq (%rsi, %rdx), %rax
	cmpq %rax, %rdi
	jae .Lmemcpy_nt
 .Lmemmove_small:
	movq %rdi, %rax
	movq %rdx, %rcx
	cmpq %rdi, %rsi
	jb .Lmemmove_backwards
	rep movsb
	ret
 .Lmemmove_backwards:
	leaq -1(%rdi, %rdx), %rdi
	leaq -1(%rsi, %rdx), %rsi
	std
	rep movsb
	cld
	ret

.align 16
.global memset
memset:
	cmpq $nt_threshold, %rdx
	jae .Lmemset_nt
	movq %rdi, %r8
	movq %rdx, %rcx
	movzbl %sil, %eax
	rep stosb
	movq %r8, %rax
	ret

 .Lmemset_nt:
	movq %rdi, %rax

	movzbl %sil, %esi
	imul $0x01010101, %esi
	movd %esi, %xmm0
	pshufd $0, %xmm0, %xmm0

	testq $63, %rdi
	jz .Lmemset_nt_aligned

	# NOTE: this unconditionally writes 64 bytes to align to 64 byte boundary
	#       but as nt branch is only taken for huge buffers, it doesnt add much overhead

	movdqu %xmm0,  0(%rdi)
	movdqu %xmm0, 16(%rdi)
	movdqu %xmm0, 32(%rdi)
	movdqu %xmm0, 48(%rdi)

	movq %rdi, %rcx
	andq $63, %rcx
	leaq -64(%rdx, %rcx), %rdx

	negq %rcx
	leaq 64(%rdi, %rcx), %rdi

 .Lmemset_nt_aligned:
	movq %rdx, %rcx
	shrq $6, %rdx

 .align 16
 .Lmemset_nt_loop:
	movntdq %xmm0,  0(%rdi)
	movntdq %xmm0, 16(%rdi)
	movntdq %xmm0, 32(%rdi)
	movntdq %xmm0, 48(%rdi)

	addq $64, %rdi
	subq $1, %rdx
	jnz .Lmemset_nt_loop

	andq $63, %rcx
	jnz .Lmemset_nt_bytes
	sfence
	ret

 .Lmemset_nt_bytes:
	movq %rax, %rdx
	movzbl %sil, %eax
	rep stosb
	movq %rdx, %rax
	sfence
	ret

.align 16
.global memchr
memchr:
	testq %rdx, %rdx
	jz .Lmemchr_no_match

	movzbl %sil, %esi
	imul $0x01010101, %esi
	movd %esi, %xmm0
	pshufd $0, %xmm0, %xmm0

	movq %rdi, %rcx
	andq $15, %rcx
	jz .Lmemchr_loop

	movq %rdi, %rsi
	subq %rcx, %rsi
	movdqa (%rsi), %xmm1
	pcmpeqb %xmm0, %xmm1
	pmovmskb %xmm1, %eax
	shrl %cl, %eax
	jnz .Lmemchr_match

	leaq 16(%rsi), %rdi

	addq %rcx, %rdx
	subq $16, %rdx
	jbe .Lmemchr_no_match

 .align 16
 .Lmemchr_loop:
	movdqa (%rdi), %xmm1
	pcmpeqb %xmm0, %xmm1
	pmovmskb %xmm1, %eax
	testl %eax, %eax
	jnz .Lmemchr_match

	addq $16, %rdi
	subq $16, %rdx
	ja .Lmemchr_loop

 .Lmemchr_no_match:
	xorq %rax, %rax
	ret

 .Lmemchr_match:
	bsfl %eax, %eax
	cmpq %rdx, %rax
	jae .Lmemchr_no_match
	addq %rdi, %rax
	ret

.align 16
.global memcmp
memcmp:
	testq %rdx, %rdx
	jz .Lmemcmp_equal

	movq %rdi, %rax
	movq %rsi, %rcx
	andq $15, %rax
	andq $15, %rcx
	cmpq %rax, %rcx
	cmovaq %rcx, %rax

	testq %rax, %rax
	jz .Lmemcmp_loop

	movq $16, %rcx
	subq %rax, %rcx

	cmpq %rcx, %rdx
	cmovbq %rdx, %rcx

	subq %rcx, %rdx

 .Lmemcmp_align_loop:
	movzbl (%rdi), %eax
	movzbl (%rsi), %r8d
	subl %r8d, %eax
	jnz .Lmemcmp_return

	addq $1, %rdi
	addq $1, %rsi
	subq $1, %rcx
	jnz .Lmemcmp_align_loop

 .align 16
 .Lmemcmp_loop:
	movdqu (%rdi), %xmm0
	movdqu (%rsi), %xmm1
	pcmpeqb %xmm0, %xmm1
	pmovmskb %xmm1, %eax
	xorl $0xFFFF, %eax
	jnz .Lmemcmp_differ

	addq $16, %rdi
	addq $16, %rsi
	subq $16, %rdx
	ja .Lmemcmp_loop

 .Lmemcmp_equal:
	xorl %eax, %eax
 .Lmemcmp_return:
	ret

 .Lmemcmp_differ:
	bsfl %eax, %ecx
	cmpq %rdx, %rcx
	jae .Lmemcmp_equal
	movzbl (%rdi, %rcx), %eax
	movzbl (%rsi, %rcx), %edx
	subl %edx, %eax
	ret

.align 16
.global strlen
strlen:
	movq %rdi, %rsi

	pxor %xmm0, %xmm0

	movq %rsi, %rcx
	andq $15, %rcx
	jz .Lstrlen_loop

	movq %rsi, %rdx
	subq %rcx, %rdx
	movdqa (%rdx), %xmm1
	pcmpeqb %xmm0, %xmm1
	pmovmskb %xmm1, %eax
	shrl %cl, %eax
	jnz .Lstrlen_null_found

	leaq 16(%rdx), %rsi

 .align 16
 .Lstrlen_loop:
	movdqa (%rsi), %xmm1
	pcmpeqb %xmm0, %xmm1
	pmovmskb %xmm1, %eax
	testl %eax, %eax
	jnz .Lstrlen_null_found

	addq $16, %rsi
	jmp .Lstrlen_loop

 .Lstrlen_null_found:
	bsfl %eax, %eax
	addq %rsi, %rax
	subq %rdi, %rax
	ret
