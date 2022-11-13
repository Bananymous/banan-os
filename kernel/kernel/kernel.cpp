#include <kernel/tty.h>

#include <stdio.h>
#include <stdint.h>


extern "C"
void kernel_main()
{
	asm volatile("cli");


	terminal_initialize();

	printf("Hello from the kernel!\n");

	printf("%p\n", kernel_main);

	int a = 10;
	printf("%p\n", &a);
}