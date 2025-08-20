#include <setjmp.h>
#include <signal.h>

static_assert(sizeof(sigjmp_buf) == (_JMP_BUF_REGS + 1) * sizeof(long) + sizeof(sigset_t));

void siglongjmp(sigjmp_buf env, int val)
{
	if (env[_JMP_BUF_REGS])
		pthread_sigmask(SIG_SETMASK, reinterpret_cast<sigset_t*>(&env[_JMP_BUF_REGS + 1]), nullptr);
	longjmp(env, val);
}
