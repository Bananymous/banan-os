#ifndef _STDIO_H
#define _STDIO_H 1

// https://pubs.opengroup.org/onlinepubs/9699919799/basedefs/stdio.h.html

#include <sys/cdefs.h>

__BEGIN_DECLS

#define __need_off_t
#define __need_ssize_t
#include <sys/types.h>

#ifndef __va_list_defined
	#define __va_list_defined
	#define __need___va_list
	#include <stdarg.h>
	typedef __gnuc_va_list va_list;
#endif

#define __need_size_t
#define __need_NULL
#include <stddef.h>

#include <bits/types/FILE.h>

typedef off_t fpos_t;

#define BUFSIZ 1024
#define L_ctermid 20
#define L_tmpnam 20

#define _IOFBF 1
#define _IOLBF 2
#define _IONBF 3

// NOTE: also defined in fcntl.h
#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2

#define FILENAME_MAX 256
#define FOPEN_MAX 16
#define TMP_MAX 10000

#define EOF (-1)

#define P_tmpdir "/tmp"

extern FILE* __stdin;
#define stdin __stdin
extern FILE* __stdout;
#define stdout __stdout
extern FILE* __stderr;
#define stderr __stderr
extern FILE* __stddbg;
#define stddbg __stddbg

int		asprintf(char** __restrict ptr, const char* __restrict format, ...);
void	clearerr(FILE* stream);
char*	ctermid(char* s);
int		dprintf(int fildes, const char* __restrict format, ...);
int		fclose(FILE* stream);
FILE*	fdopen(int fildes, const char* mode);
int		feof(FILE* stream);
int		ferror(FILE* stream);
int		fflush(FILE* stream);
int		fgetc(FILE* stream);
int		fgetpos(FILE* __restrict stream, fpos_t* __restrict pos);
char*	fgets(char* __restrict s, int n, FILE* __restrict stream);
int		fileno(FILE* stream);
void	flockfile(FILE* stream);
FILE*	fmemopen(void* __restrict buf, size_t size, const char* __restrict mode);
FILE*	fopen(const char* __restrict pathname, const char* __restrict mode);
int		fprintf(FILE* __restrict stream, const char* __restrict format, ...);
int		fputc(int c, FILE* stream);
int		fputs(const char* __restrict s, FILE* __restrict stream);
size_t	fread(void* __restrict buf, size_t size, size_t nitems, FILE* __restrict stream);
FILE*	freopen(const char* __restrict pathname, const char* __restrict mode, FILE* __restrict stream);
int		fscanf(FILE* __restrict stream, const char* __restrict format, ...);
int		fseek(FILE* stream, long offset, int whence);
int		fseeko(FILE* stream, off_t offset, int whence);
int		fsetpos(FILE* stream, const fpos_t* pos);
long	ftell(FILE* stream);
off_t	ftello(FILE* stream);
int		ftrylockfile(FILE* stream);
void	funlockfile(FILE* stream);
size_t	fwrite(const void* __restrict ptr, size_t size, size_t nitems, FILE* __restrict stream);
int		getc(FILE* stream);
int		getchar(void);
int		getc_unlocked(FILE* stream);
int		getchar_unlocked(void);
ssize_t	getdelim(char** __restrict lineptr, size_t* __restrict n, int delimeter, FILE* __restrict stream);
ssize_t	getline(char** __restrict lineptr, size_t* __restrict n, FILE* __restrict stream);
char*	gets(char* s);
FILE*	open_memstream(char** bufp, size_t* sizep);
int		pclose(FILE* stream);
void	perror(const char* s);
FILE*	popen(const char* command, const char* mode);
int		printf(const char* __restrict format, ...);
int		putc(int c, FILE* stream);
int		putchar(int c);
int		putc_unlocked(int c, FILE* stream);
int		putchar_unlocked(int c);
int		puts(const char* s);
int		remove(const char* path);
int		rename(const char* old, const char* _new);
int		renameat(int oldfd, const char* old, int newfd, const char* _new);
void	rewind(FILE* stream);
int		scanf(const char* __restrict format, ...);
void	setbuf(FILE* __restrict stream, char* __restrict buf);
int		setvbuf(FILE* __restrict stream, char* __restrict buf, int type, size_t size);
int		snprintf(char* __restrict s, size_t n, const char* __restrict format, ...);
int		sprintf(char* __restrict s, const char* __restrict format, ...);
int		sscanf(const char* __restrict s, const char* __restrict format, ...);
char*	tempnam(const char* dir, const char* pfx);
FILE*	tmpfile(void);
char*	tmpnam(char* s);
int		ungetc(int c, FILE* stream);
int		vasprintf(char** __restrict ptr, const char* __restrict format, va_list ap);
int		vdprintf(int fildes, const char* __restrict format, va_list ap);
int		vfprintf(FILE* __restrict stream, const char* __restrict format, va_list ap);
int		vfscanf(FILE* __restrict stream, const char* __restrict format, va_list arg);
int		vprintf(const char* __restrict format, va_list ap);
int		vscanf(const char* __restrict format, va_list arg);
int		vsnprintf(char* __restrict s, size_t n, const char* __restrict format, va_list ap);
int		vsprintf(char* __restrict s, const char* __restrict format, va_list ap);
int		vsscanf(const char* __restrict s, const char* __restrict format, va_list arg);

__END_DECLS

#endif
