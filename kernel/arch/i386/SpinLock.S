.global spinlock_lock_asm
spinlock_lock_asm:
	lock; btsl $0, (%edi)
	jnc .done
.retry:
	pause
	testl $1, (%edi)
	jne .retry
	lock; btsl $0, (%edi)
	jc .retry
.done:
	ret

.global spinlock_unlock_asm
spinlock_unlock_asm:
	movl $0, (%edi)
	ret