# bool safe_user_memcpy(void*, const void*, size_t)
.global safe_user_memcpy
.global safe_user_memcpy_end
.global safe_user_memcpy_fault
safe_user_memcpy:
	xorq %rax, %rax
	movq %rdx, %rcx
	rep movsb
	incq %rax
safe_user_memcpy_fault:
	ret
safe_user_memcpy_end:

# bool safe_user_strncpy(void*, const void*, size_t)
.global safe_user_strncpy
.global safe_user_strncpy_end
.global safe_user_strncpy_fault
safe_user_strncpy:
	movq %rdx, %rcx
	testq %rcx, %rcx
	jz safe_user_strncpy_fault

 .safe_user_strncpy_align_loop:
	testb $0x7, %sil
	jz .safe_user_strncpy_align_done

	movb (%rsi), %al
	movb %al, (%rdi)
	testb %al, %al
	jz .safe_user_strncpy_done

	incq %rdi
	incq %rsi
	decq %rcx
	jnz .safe_user_strncpy_align_loop
	jmp safe_user_strncpy_fault

 .safe_user_strncpy_align_done:
	movq $0x0101010101010101, %r8
	movq $0x8080808080808080, %r9

 .safe_user_strncpy_qword_loop:
	cmpq $8, %rcx
	jb .safe_user_strncpy_qword_done

	movq (%rsi), %rax
	movq %rax, %r10
	movq %rax, %r11

	# https://graphics.stanford.edu/~seander/bithacks.html#ZeroInWord
	subq %r8, %r10
	notq %r11
	andq %r11, %r10
	andq %r9, %r10
	jnz .safe_user_strncpy_byte_loop

	movq %rax, (%rdi)

	addq $8, %rdi
	addq $8, %rsi
	subq $8, %rcx
	jnz .safe_user_strncpy_qword_loop
	jmp safe_user_strncpy_fault

 .safe_user_strncpy_qword_done:
	testq %rcx, %rcx
	jz safe_user_strncpy_fault

 .safe_user_strncpy_byte_loop:
	movb (%rsi), %al
	movb %al, (%rdi)
	testb %al, %al
	jz .safe_user_strncpy_done

	incq %rdi
	incq %rsi
	decq %rcx
	jnz .safe_user_strncpy_byte_loop

 safe_user_strncpy_fault:
	xorq %rax, %rax
	ret

 .safe_user_strncpy_done:
	movb $1, %al
	ret
safe_user_strncpy_end:
