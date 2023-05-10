#pragma onces

#include <sys/cdefs.h>

__BEGIN_DECLS

typedef int sig_atomic_t;

int		raise(int);
void	(*signal(int, void (*)(int)))(int);

__END_DECLS
