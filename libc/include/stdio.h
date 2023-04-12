#pragma once

#include <stddef.h>
#include <stdarg.h>
#include <sys/cdefs.h>
#include <sys/types.h>

#define EOF (-1)

#define stdin stdin
#define stdout stdout
#define stderr stderr

#define SEEK_CUR 0
#define SEEK_END 1
#define SEEK_SET 2

#define EXIT_FAILURE 1
#define EXIT_SUCCESS 0

__BEGIN_DECLS

struct FILE;
typedef struct FILE FILE;

extern FILE* stdin;
extern FILE* stdout;
extern FILE* stderr;

FILE* fopen(const char* __restrict__, const char* __restrict__);
int fclose(FILE*);
int fflush(FILE*);
int fprintf(FILE* __restrict__, const char* __restrict__, ...);
int fseek(FILE*, long, int);
int printf(const char* __restrict__, ...);
int putchar(int);
int puts(const char*);
int vfprintf(FILE* __restrict__, const char* __restrict__, va_list);
long ftell(FILE*);
size_t fread(void* __restrict__, size_t, size_t, FILE*);
size_t fwrite(void const* __restrict__, size_t, size_t, FILE*);
void setbuf(FILE* __restrict__, char* __restrict__);
int sprintf(char* __restrict__, const char* __restrict__, ...);

__END_DECLS
