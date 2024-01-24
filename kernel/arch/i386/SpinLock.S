.global spinlock_lock_asm
spinlock_lock_asm:
	movl 4(%esp), %eax
	lock; btsl $0, (%eax)
	jnc .done
.retry:
	pause
	testl $1, (%eax)
	jne .retry
	lock; btsl $0, (%eax)
	jc .retry
.done:
	ret

.global spinlock_unlock_asm
spinlock_unlock_asm:
	movl 4(%esp), %eax
	movl $0, (%eax)
	ret
