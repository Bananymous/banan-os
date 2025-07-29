#ifndef _STDLIB_H
#define _STDLIB_H 1

// https://pubs.opengroup.org/onlinepubs/9699919799/basedefs/stdlib.h.html

#include <sys/cdefs.h>

__BEGIN_DECLS

#include <limits.h>
#include <sys/wait.h>

#define __need_NULL
#define __need_size_t
#define __need_wchar_t
#include <stddef.h>

#define EXIT_FAILURE 1
#define EXIT_SUCCESS 0

#define RAND_MAX INT_MAX

#define MB_CUR_MAX ((size_t)4)

typedef struct
{
	int quot;	/* quotient */
	int rem;	/* remainder */
} div_t;

typedef struct
{
	long quot;	/* quotient */
	long rem;	/* remainder */
} ldiv_t;

typedef struct
{
	long long quot;	/* quotient */
	long long rem;	/* remainder */
} lldiv_t;

void				_Exit(int status) __attribute__((__noreturn__));
long				a64l(const char* s);
void				abort(void) __attribute__((__noreturn__));
int					abs(int i);
void*				aligned_alloc(size_t alignment, size_t size);
int					atexit(void (*func)(void));
double				atof(const char* str);
int					atoi(const char* str);
long				atol(const char* str);
long long			atoll(const char* str);
void*				bsearch(const void* key, const void* base, size_t nel, size_t width, int (*compar)(const void*, const void*));
void*				calloc(size_t nelem, size_t elsize);
int					clearenv(void);
div_t				div(int numer, int denom);
double				drand48(void);
double				erand48(unsigned short xsubi[3]);
void				exit(int status);
void				free(void* ptr);
char*				getenv(const char* name);
int					getsubopt(char** optionp, char* const* keylistp, char** valuep);
int					grantpt(int fildes);
char*				initstate(unsigned seed, char* state, size_t size);
long				jrand48(unsigned short xsubi[3]);
char*				l64a(long value);
long				labs(long i);
void				lcong48(unsigned short param[7]);
ldiv_t				ldiv(long numer, long denom);
long long			llabs(long long i);
lldiv_t				lldiv(long long numer, long long denom);
long				lrand48(void);
void*				malloc(size_t size);
int					mblen(const char* s, size_t n);
size_t				mbstowcs(wchar_t* __restrict pwcs, const char* __restrict s, size_t n);
int					mbtowc(wchar_t* __restrict pwc, const char* __restrict s, size_t n);
char*				mkdtemp(char* _template);
char*				mktemp(char* _template);
int					mkstemp(char* _template);
long				mrand48(void);
long				nrand48(unsigned short xsubi[3]);
int					posix_memalign(void** memptr, size_t alignment, size_t size);
int					posix_openpt(int oflag);
char*				ptsname(int fildes);
int					putenv(char* string);
void				qsort(void* base, size_t nel, size_t width, int (*compar)(const void*, const void*));
int					rand(void);
int					rand_r(unsigned* seed);
long				random(void);
void*				realloc(void* ptr, size_t size);
char*				realpath(const char* __restrict file_name, char* __restrict resolved_name);
unsigned short*		seed48(unsigned short seed16v[3]);
int					setenv(const char* envname, const char* envval, int overwrite);
void				setkey(const char* key);
char*				setstate(char* state);
void				srand(unsigned seed);
void				srand48(long seedval);
void				srandom(unsigned seed);
double				strtod(const char* __restrict nptr, char** __restrict endptr);
float				strtof(const char* __restrict nptr, char** __restrict endptr);
long				strtol(const char* __restrict nptr, char** __restrict endptr, int base);
long double			strtold(const char* __restrict nptr, char** __restrict endptr);
long long			strtoll(const char* __restrict nptr, char** __restrict endptr, int base);
unsigned long		strtoul(const char* __restrict nptr, char** __restrict endptr, int base);
unsigned long long	strtoull(const char* __restrict nptr, char** __restrict endptr, int base);
int					system(const char* command);
int					unlockpt(int fildes);
int					unsetenv(const char* name);
size_t				wcstombs(char* __restrict s, const wchar_t* __restrict pwcs, size_t n);
int					wctomb(char* s, wchar_t wchar);

__END_DECLS

#endif
