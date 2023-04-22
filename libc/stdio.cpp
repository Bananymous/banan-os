#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <sys/syscall.h>
#include <unistd.h>

struct FILE
{
	int fd;
};

static FILE __stdin { .fd = STDIN_FILENO };
static FILE __stdout { .fd = STDOUT_FILENO };
static FILE __stderr { .fd = STDERR_FILENO };

FILE* stdin = &__stdin;
FILE* stdout = &__stdout;
FILE* stderr = &__stderr;

int fclose(FILE*)
{
	return -1;
}

int fflush(FILE*)
{
	return 0;
}

FILE* fopen(const char* __restrict__, const char* __restrict__)
{
	return nullptr;
}

int fseek(FILE*, long, int)
{
	return -1;
}

long ftell(FILE*)
{
	return -1;
}

size_t fread(void* __restrict__, size_t, size_t, FILE*)
{
	return 0;
}

size_t fwrite(void const* __restrict__, size_t, size_t, FILE*)
{
	return 0;
}

int fprintf(FILE* __restrict__ file, const char* __restrict__ format, ...)
{
	va_list args;
	va_start(args, format);
	int res = vfprintf(stdout, format, args);
	va_end(args);
	return res;
}


void setbuf(FILE* __restrict__, char* __restrict__)
{

}

int vfprintf(FILE* __restrict__, const char* __restrict__, va_list)
{
	return -1;
}

int printf(const char* __restrict__ format, ...)
{
	va_list args;
	va_start(args, format);
	int res = vfprintf(stdout, format, args);
	va_end(args);
	return res;
}

int putchar(int ch)
{
	return printf("%c", ch);
}

int puts(const char* str)
{
	return syscall(SYS_WRITE, STDOUT_FILENO, str, strlen(str));
}

int sprintf(char* __restrict__ stream, const char* __restrict__ format, ...)
{
	return -1;
}
