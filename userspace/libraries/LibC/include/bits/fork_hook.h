#ifndef _BITS_FORK_HOOK_H
#define _BITS_FORK_HOOK_H 1

#include <sys/cdefs.h>

__BEGIN_DECLS

#include <bits/types/pthread_types.h>

#define _FORK_HOOK_CONCAT_IMPL(a, b) a##b
#define _FORK_HOOK_CONCAT(a, b) _FORK_HOOK_CONCAT_IMPL(a, b)

struct fork_hook_t
{
	int priority;
	void (*prepare)();
	void (*parent)();
	void (*child)();
};

#define FORK_HOOK(prio, prepare, parent, child) \
	__attribute__((used, retain, section("fork_hooks"))) \
	static const fork_hook_t _FORK_HOOK_CONCAT(s_fork_hook_, __LINE__) = { prio, prepare, parent, child }

#define FORK_SAFE_MUTEX(prio, name, init) \
	static pthread_mutex_t name = init; \
	static void s_fork_hook_##name##_prepare(void) { pthread_mutex_lock(&name); } \
	static void s_fork_hook_##name##_parent(void) { pthread_mutex_unlock(&name); } \
	static void s_fork_hook_##name##_child(void) { name = init; } \
	FORK_HOOK(prio, s_fork_hook_##name##_prepare, s_fork_hook_##name##_parent, s_fork_hook_##name##_child)

__END_DECLS

#endif
