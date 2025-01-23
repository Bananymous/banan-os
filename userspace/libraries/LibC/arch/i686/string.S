.global memchr
memchr:
	xchgl 4(%esp), %edi
	movl  8(%esp), %eax
	movl 12(%esp), %ecx
	movl $1, %edx
	cmpl $1, %ecx # clear ZF if count is zero
	repne scasb
	cmovel %edi, %edx
	leal -1(%edx), %eax
	movl 4(%esp), %edi
	ret

.global memcmp
memcmp:
	xchgl 4(%esp), %edi
	xchgl 8(%esp), %esi
	movl 12(%esp), %ecx
	testl %ecx, %ecx # set ZF if count is zero
	repe cmpsb
	jne .memcmp_not_equal
	xorl %eax, %eax
	jmp .memcmp_done
 .memcmp_not_equal:
	movzbl -1(%edi), %eax
	movzbl -1(%esi), %ecx
	subl %ecx, %eax
 .memcmp_done:
	movl 4(%esp), %edi
	movl 8(%esp), %esi
	ret

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

.global strlen
strlen:
	xchgl 4(%esp), %edi
	xorb %al, %al
	movl $-1, %ecx
	repne scasb
	movl 4(%esp), %edi
	movl $-2, %eax
	subl %ecx, %eax
	ret
