#include <setjmp.h>
#include <stdio.h>

jmp_buf env;

void iterate()
{
	static volatile int count = 0;
	if (count < 10)
	{
		count = count + 1;
		longjmp(env, count);
	}
}

int main()
{
	printf("start\n");
	if (int ret = setjmp(env))
		printf("longjmp %d\n", ret);
	iterate();
}
