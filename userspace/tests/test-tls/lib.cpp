#include <stdio.h>

extern thread_local int tls_in_executable1;
extern thread_local int tls_in_executable2;

thread_local int tls_in_library1 = 1;
thread_local int tls_in_library2 = 1;

void test_tls_exec_lib1()
{
	printf("  exec in lib:  %d (%p)\n", tls_in_executable1, &tls_in_executable1);
	tls_in_executable1 = 3;
	printf("  exec in lib:  %d (%p)\n", tls_in_executable1, &tls_in_executable1);
}

void test_tls_exec_lib2()
{
	printf("  exec in lib:  %d (%p)\n", tls_in_executable2, &tls_in_executable2);
	tls_in_executable2 = 3;
	printf("  exec in lib:  %d (%p)\n", tls_in_executable2, &tls_in_executable2);
}

void test_tls_lib_lib1()
{
	printf("  lib in lib:  %d (%p)\n", tls_in_library1, &tls_in_library1);
	tls_in_library1 = 3;
	printf("  lib in lib:  %d (%p)\n", tls_in_library1, &tls_in_library1);
}

void test_tls_lib_lib2()
{
	printf("  lib in lib:  %d (%p)\n", tls_in_library2, &tls_in_library2);
	tls_in_library2 = 3;
	printf("  lib in lib:  %d (%p)\n", tls_in_library2, &tls_in_library2);
}
