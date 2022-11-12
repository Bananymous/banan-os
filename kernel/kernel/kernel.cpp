#include <kernel/tty.h>

#include <stdio.h>

extern "C"
void kernel_main()
{
	terminal_initialize();
	printf("Hello from the kernel!\n");
}