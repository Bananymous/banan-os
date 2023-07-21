#ifndef _SIGNAL_H
#define _SIGNAL_H 1

// https://pubs.opengroup.org/onlinepubs/9699919799/basedefs/signal.h.html

#include <sys/cdefs.h>

__BEGIN_DECLS

#include <time.h>

#define SIG_DFL		((void (*)(int))0)
#define SIG_ERR		((void (*)(int))1)
#define SIG_HOLD	((void (*)(int))2)
#define SIG_IGN		((void (*)(int))3)

#define __need_pthread_t
#define __need_size_t
#define __need_uid_t
#define __need_pid_t
#define __need_pthread_attr_t
#include <sys/types.h>

typedef int sig_atomic_t;
typedef void* sigset_t;

union sigval
{
	int		sival_int; /* Integer signal value. */
	void*	sival_ptr; /* Pointer signal value. */
};

struct sigevent
{
	int				sigev_notify;					/* Notification type. */
	int				sigev_signo;					/* Signal number. */
	union sigval	sigev_value;					/* Signal value. */
	void (*sigev_notify_function)(union sigval);	/* Notification function. */
	pthread_attr_t*	sigev_notify_attributes;  		/* Notification attributes. */
};

#define SIGEV_NONE		1
#define SIGEV_SIGNAL	2
#define SIGEV_THREAD	3

#define SIGABRT 	1
#define SIGALRM 	2
#define SIGBUS 		3
#define SIGCHLD 	4
#define SIGCONT 	5
#define SIGFPE 		6
#define SIGHUP 		7
#define SIGILL 		8
#define SIGINT 		9
#define SIGKILL 	10
#define SIGPIPE 	11
#define SIGQUIT 	12
#define SIGSEGV 	13
#define SIGSTOP 	14
#define SIGTERM 	15
#define SIGTSTP 	16
#define SIGTTIN 	17
#define SIGTTOU 	18
#define SIGUSR1 	19
#define SIGUSR2 	20
#define SIGPOLL 	21
#define SIGPROF 	22
#define SIGSYS 		23
#define SIGTRAP 	24
#define SIGURG 		25
#define SIGVTALRM 	26
#define SIGXCPU 	27
#define SIGXFSZ 	28
#define SIGRTMIN	29
#define SIGRTMAX	(SIGRTMIN+32)

#define _SIGMIN SIGABRT
#define _SIGMAX SIGRTMAX

#define SIG_BLOCK	1
#define SIG_UNBLOCK	2
#define SIG_SETMASK	3

#define SA_NOCLDSTOP	0x001
#define SA_ONSTACK		0x002
#define SA_RESETHAND	0x004
#define SA_RESTART		0x008
#define SA_SIGINFO		0x010
#define SA_NOCLDWAIT	0x020
#define SA_NODEFER		0x040
#define SS_ONSTACK		0x080
#define SS_DISABLE		0x100
#define MINSIGSTKSZ		0x200
#define SIGSTKSZ		0x400

typedef struct
{
	void**	ss_sp;		/* Stack base or pointer. */
	size_t	ss_size;	/* Stack size. */
	int		ss_flags;	/* Flags. */
} stack_t;

typedef struct {} mcontext_t;

typedef struct __ucontext_t
{
	struct __ucontext_t*	uc_link;		/* Pointer to the context that is resumed when this context returns. */
	sigset_t				uc_sigmask;		/* The set of signals that are blocked when this context is active. */
	stack_t					uc_stack;		/* The stack used by this context. */
	mcontext_t				uc_mcontext;	/* A machine-specific representation of the saved context. */
} ucontext_t;

typedef struct
{
	int				si_signo;	/* Signal number. */
	int				si_code;	/* Signal code. */
	int				si_errno;	/* If non-zero, an errno value associated with this signal, as described in <errno.h>. */
	pid_t			si_pid;		/* Sending process ID. */
	uid_t			si_uid;		/* Real user ID of sending process. */
	void*			si_addr;	/* Address of faulting instruction. */
	int				si_status;	/* Exit value or signal. */
	long			si_band;	/* Band event for SIGPOLL. */
	union sigval	si_value;	/* Signal value. */
} siginfo_t;

struct sigaction
{
	void (*sa_handler)(int);						/* Pointer to a signal-catching function or one of the SIG_IGN or SIG_DFL. */
	sigset_t	sa_mask;							/* Set of signals to be blocked during execution of the signal handling function. */
	int			sa_flags;							/* Special flags. */
	void (*sa_sigaction)(int, siginfo_t*, void*);	/* Pointer to a signal-catching function. */
};

// TODO: The <signal.h> header shall define the symbolic constants in the
//       Code column of the following table for use as values of si_code
//       that are signal-specific or non-signal-specific reasons why the
//       signal was generated.

int		kill(pid_t pid, int sig);
int		killpg(pid_t pgpr, int sig);
void	psiginfo(const siginfo_t* pinfo, const char* message);
void	psignal(int signum, const char* message);
int		pthread_kill(pthread_t thread, int sig);
int		pthread_sigmask(int how, const sigset_t* __restrict set, sigset_t* __restrict oset);
int		raise(int sig);
int		sigaction(int sig, const struct sigaction* __restrict act, struct sigaction* __restrict oact);
int		sigaddset(sigset_t* set, int signo);
int		sigaltstack(const stack_t* __restrict ss, stack_t* __restrict oss);
int		sigdelset(sigset_t* set, int signo);
int		sigemptyset(sigset_t* set);
int		sigfillset(sigset_t* set);
int		sighold(int sig);
int		sigignore(int sig);
int		siginterrupt(int sig, int flag);
int		sigismember(const sigset_t* set, int signo);
void	(*signal(int sig, void (*func)(int)))(int);
int		sigpause(int sig);
int		sigpending(sigset_t* set);
int		sigprocmask(int how, const sigset_t* __restrict set, sigset_t* __restrict oset);
int		sigqueue(pid_t pid, int signo, union sigval value);
int		sigrelse(int sig);
void	(*sigset(int sig, void (*disp)(int)))(int);
int		sigsuspend(const sigset_t* sigmask);
int		sigtimedwait(const sigset_t* __restrict set, siginfo_t* __restrict info, const struct timespec* __restrict timeout);
int		sigwait(const sigset_t* __restrict set, int* __restrict sig);
int		sigwaitinfo(const sigset_t* __restrict set, siginfo_t* __restrict info);

__END_DECLS

#endif
