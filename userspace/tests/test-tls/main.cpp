#include <stdio.h>
#include <pthread.h>

extern void test_tls_exec_lib1();
extern void test_tls_exec_lib2();

extern void test_tls_lib_lib1();
extern void test_tls_lib_lib2();

extern thread_local int tls_in_library1;
extern thread_local int tls_in_library2;

thread_local int tls_in_executable1 = 1;
thread_local int tls_in_executable2 = 1;

void run_test()
{
	printf("running on thread %d (expected 1, 2, 2, 3)\n", pthread_self());
	printf("  exec in exec: %d (%p)\n", tls_in_executable1, &tls_in_executable1);
	tls_in_executable1 = 2;
	printf("  exec in exec: %d (%p)\n", tls_in_executable1, &tls_in_executable1);
	test_tls_exec_lib1();

	printf("running on thread %d (expected 1, 2, 2, 3)\n", pthread_self());
	printf("  exec in exec: %d (%p)\n", tls_in_executable2, &tls_in_executable2);
	tls_in_executable2 = 2;
	printf("  exec in exec: %d (%p)\n", tls_in_executable2, &tls_in_executable2);
	test_tls_exec_lib2();

	printf("running on thread %d (expected 1, 2, 2, 3)\n", pthread_self());
	printf("  lib in exec: %d (%p)\n", tls_in_library1, &tls_in_library1);
	tls_in_library1 = 2;
	printf("  lib in exec: %d (%p)\n", tls_in_library1, &tls_in_library1);
	test_tls_lib_lib1();

	printf("running on thread %d (expected 1, 2, 2, 3)\n", pthread_self());
	printf("  lib in exec: %d (%p)\n", tls_in_library2, &tls_in_library2);
	tls_in_library2 = 2;
	printf("  lib in exec: %d (%p)\n", tls_in_library2, &tls_in_library2);
	test_tls_lib_lib2();
}

int main(int argc, char** argv)
{
	run_test();

#if 0
	pthread_t tid;
	pthread_create(&tid, nullptr,
		[](void*) -> void* {
			run_test();
			return nullptr;
		}, nullptr
	);
	pthread_join(tid, nullptr);
#endif
}
