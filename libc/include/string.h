#pragma once

#include <stddef.h>
#include <sys/cdefs.h>

__BEGIN_DECLS

int memcmp(const void*, const void*, size_t);
void* memcpy(void* __restrict__, const void* __restrict__, size_t);
void* memmove(void*, const void*, size_t);
void* memset(void*, int, size_t);
size_t strlen(const char*);

int strcmp(const char*, const char*);
int strncmp(const char*, const char*, size_t);

char* strcpy(char* __restrict__, const char* __restrict__);
char* strncpy(char* __restrict__, const char* __restrict__, size_t);

char* strcat(char* __restrict__, const char* __restrict__);

char* strchr(const char*, int);

char* strstr(const char*, const char*);

char* strerror(int);
const char* strerrorname_np(int);
const char* strerrordesc_np(int);

__END_DECLS