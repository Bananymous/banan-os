#include <stdio.h>

struct foo_t
{
	foo_t() { printf("global constructor works\n"); }
	~foo_t() { printf("global destructor works\n"); }
};

foo_t foo;

int main()
{
	printf("main\n");
}
