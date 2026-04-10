.align 16
.global memcpy
memcpy:
	movq %rdi, %rax
	movq %rdx, %rcx
	rep movsb
	ret

.align 16
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

.align 16
.global memset
memset:
	movq %rdi, %r8
	movb %sil, %al
	movq %rdx, %rcx
	rep stosb
	movq %r8, %rax
	ret
