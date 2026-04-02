# bool safe_user_memcpy(void*, const void*, size_t)
.global safe_user_memcpy
.global safe_user_memcpy_end
.global safe_user_memcpy_fault
safe_user_memcpy:
	xorl %eax, %eax
	xchgl 4(%esp), %edi
	xchgl 8(%esp), %esi
	movl 12(%esp), %ecx
	movl %edi, %edx
	rep movsb
	movl 4(%esp), %edi
	movl 8(%esp), %esi
	incl %eax
safe_user_memcpy_fault:
	ret
safe_user_memcpy_end:

# bool safe_user_strncpy(void*, const void*, size_t)
.global safe_user_strncpy
.global safe_user_strncpy_end
.global safe_user_strncpy_fault
safe_user_strncpy:
	xchgl 4(%esp), %edi
	xchgl 8(%esp), %esi
	movl 12(%esp), %ecx

	testl %ecx, %ecx
	jz safe_user_strncpy_fault

 .safe_user_strncpy_loop:
	movb (%esi), %al
	movb %al, (%edi)
	testb %al, %al
	jz .safe_user_strncpy_done

	incl %edi
	incl %esi
	decl %ecx
	jnz .safe_user_strncpy_loop

 safe_user_strncpy_fault:
	xorl %eax, %eax
	jmp .safe_user_strncpy_return

 .safe_user_strncpy_done:
	movl $1, %eax

 .safe_user_strncpy_return:
	movl 4(%esp), %edi
	movl 8(%esp), %esi
	ret

safe_user_strncpy_end:
