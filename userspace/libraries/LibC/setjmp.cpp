#include <setjmp.h>
#include <signal.h>

static_assert(sizeof(sigjmp_buf) == sizeof(jmp_buf) + sizeof(long) + sizeof(sigset_t));

void siglongjmp(sigjmp_buf env, int val)
{
	if (env[2])
		pthread_sigmask(SIG_SETMASK, reinterpret_cast<sigset_t*>(&env[3]), nullptr);
	return longjmp(env, val);
}

int sigsetjmp(sigjmp_buf env, int savemask)
{
	env[2] = savemask;
	if (savemask)
		pthread_sigmask(0, nullptr, reinterpret_cast<sigset_t*>(&env[3]));
	return setjmp(env);
}
