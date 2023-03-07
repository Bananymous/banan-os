.global spinlock_lock_asm
spinlock_lock_asm:
	lock; btsq $0, (%rdi)
	jnc .done
.retry:
	pause
	testq $1, (%rdi)
	jne .retry
	lock; btsq $0, (%rdi)
	jc .retry
.done:
	ret

.global spinlock_unlock_asm
spinlock_unlock_asm:
	movl $0, (%rdi)
	ret