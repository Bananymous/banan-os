#include <BAN/Assert.h>
#include <sys/resource.h>

int getrusage(int, struct rusage*)
{
	ASSERT_NOT_REACHED();
}
