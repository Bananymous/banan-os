#ifndef _BITS_TYPES_UTHREAD_H
#define _BITS_TYPES_UTHREAD_H 1

#include <sys/cdefs.h>

__BEGIN_DECLS

#define __need_size_t
#include <sys/types.h>

#include <bits/types/pthread_t.h>
#include <stdint.h>

typedef struct _pthread_cleanup_t
{
	void (*routine)(void*);
	void* arg;
	struct _pthread_cleanup_t* next;
} _pthread_cleanup_t;

typedef struct _dynamic_tls_entry_t
{
	void* master_addr;
	size_t master_size;
} _dynamic_tls_entry_t;

typedef struct _dynamic_tls_t
{
	int lock;
	size_t entry_count;
	_dynamic_tls_entry_t* entries;
} _dynamic_tls_t;

struct uthread
{
	struct uthread* self;
	void* master_tls_addr;
	size_t master_tls_size;
	size_t master_tls_module_count;
	_dynamic_tls_t* dynamic_tls;
	_pthread_cleanup_t* cleanup_stack;
	pthread_t id;
	int errno_;
	int cancel_type;
	int cancel_state;
	volatile int canceled;
	// FIXME: make this dynamic
	uintptr_t dtv[1 + 256];
};

#if defined(__x86_64__)
#define _get_uthread() ({ \
		struct uthread* _uthread; \
		__asm__ volatile("movq %%fs:0, %0" : "=r"(_uthread)); \
		_uthread; \
	})
#elif defined(__i686__)
#define _get_uthread() ({ \
		struct uthread* _uthread; \
		__asm__ volatile("movl %%gs:0, %0" : "=r"(_uthread)); \
		_uthread; \
	})
#endif

__END_DECLS

#endif
