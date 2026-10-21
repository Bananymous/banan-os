#ifndef _BITS_TYPES_UTHREAD_H
#define _BITS_TYPES_UTHREAD_H 1

#include <sys/cdefs.h>

__BEGIN_DECLS

#define __need_pid_t
#define __need_size_t
#include <sys/types.h>

#include <bits/types/pthread_attr_t.h>
#include <bits/types/pthread_key_t.h>
#include <bits/types/pthread_t.h>
#include <limits.h>
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

	// TLS
	void* master_tls_addr;
	size_t master_tls_size;
	size_t master_tls_module_count;
	_dynamic_tls_t* dynamic_tls;

	// LIBC
	pid_t id;
	pthread_attr_t attr;
	char name[16];
	int errno_;
	int libc_owns_stack : 1;
	int has_exited      : 1;
	int cancel_type;
	int cancel_state;
	volatile int canceled;
	_pthread_cleanup_t* cleanup_funcs;
	pthread_key_t specific_keys[PTHREAD_KEYS_MAX];
	void* specific_vals[PTHREAD_KEYS_MAX];

	// FIXME: make this dynamic
	uintptr_t dtv[1 + 256];
};

#define _get_uthread() ((struct uthread*)__builtin_thread_pointer())

__END_DECLS

#endif
