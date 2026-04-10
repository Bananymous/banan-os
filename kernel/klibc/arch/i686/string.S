.align 16
.global memcpy
memcpy:
	xchgl 4(%esp), %edi
	xchgl 8(%esp), %esi
	movl 12(%esp), %ecx
	movl %edi, %edx
	rep movsb
	movl 4(%esp), %edi
	movl 8(%esp), %esi
	movl %edx, %eax
	ret

.align 16
.global memmove
memmove:
	xchgl 4(%esp), %edi
	xchgl 8(%esp), %esi
	movl 12(%esp), %ecx
	movl %edi, %edx
	cmpl %edi, %esi
	jb .memmove_slow
	rep movsb
 .memmove_done:
	movl 4(%esp), %edi
	movl 8(%esp), %esi
	movl %edx, %eax
	ret
 .memmove_slow:
	leal -1(%edi, %ecx), %edi
	leal -1(%esi, %ecx), %esi
	std
	rep movsb
	cld
	jmp .memmove_done

.align 16
.global memset
memset:
	xchgl 4(%esp), %edi
	movl  8(%esp), %eax
	movl 12(%esp), %ecx
	movl %edi, %edx
	rep stosb
	movl 4(%esp), %edi
	movl %edx, %eax
	ret
