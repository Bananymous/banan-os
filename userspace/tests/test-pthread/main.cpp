#include <stdio.h>
#include <pthread.h>
#include <unistd.h>

void* thread_func(void*)
{
	printf("hello from thread\n");
	return nullptr;
}

int main(int argc, char** argv)
{
	pthread_t tid;

	printf("creating thread\n");

	if (pthread_create(&tid, nullptr, &thread_func, nullptr) == -1)
	{
		perror("pthread_create");
		return 1;
	}

	sleep(1);

	printf("exiting\n");

	return 0;
}
