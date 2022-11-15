#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
 
#if UINT32_MAX == UINTPTR_MAX
#define STACK_CHK_GUARD 0xe2dee396
#else
#define STACK_CHK_GUARD 0x595e9fbd94fda766
#endif
 
uintptr_t __stack_chk_guard = STACK_CHK_GUARD;

__BEGIN_DECLS

__attribute__((noreturn))
void __stack_chk_fail(void)
{
	printf("Stack smashing detected\n");
	abort();
}

__END_DECLS