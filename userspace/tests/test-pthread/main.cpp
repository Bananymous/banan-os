#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

pthread_spinlock_t spinlock;

void* thread_func(void*)
{
	printf("[THREAD] locking spinlock\n");
	pthread_spin_lock(&spinlock);
	printf("[THREAD] got spinlock\n");

	sleep(1);

	printf("[THREAD] releasing spinlock\n");
	pthread_spin_unlock(&spinlock);

	int* value = static_cast<int*>(malloc(sizeof(int)));
	if (value == nullptr)
	{
		perror("malloc");
		return nullptr;
	}

	*value = 69;

	printf("[THREAD] exiting with %d\n", *value);

	return value;
}

int main(int argc, char** argv)
{
	pthread_spin_init(&spinlock, 0);

	printf("[MAIN] locking spinlock\n");
	pthread_spin_lock(&spinlock);

	printf("[MAIN] creating thread\n");

	pthread_t thread;
	pthread_create(&thread, nullptr, &thread_func, nullptr);

	sleep(1);

	printf("[MAIN] releasing spinlock\n");
	pthread_spin_unlock(&spinlock);

	printf("[MAIN] joining thread\n");

	void* value;
	pthread_join(thread, &value);

	if (value == nullptr)
		printf("[MAIN] thread returned NULL\n");
	else
		printf("[MAIN] thread returned %d\n", *static_cast<int*>(value));

	return 0;
}
