#ifndef _INTTYPES_H
#define _INTTYPES_H 1

// https://pubs.opengroup.org/onlinepubs/9699919799/basedefs/inttypes.h.html

#include <sys/cdefs.h>

__BEGIN_DECLS

#include <stdint.h>

#define __need_wchar_t
#include <stddef.h>

typedef struct
{
	intmax_t quot;
	intmax_t rem;
} imaxdiv_t;

#ifdef __x86_64__
	#define __PRI64_PREFIX	"l"
	#define __PRIPTR_PREFIX	"l"
#else
	#define __PRI64_PREFIX	"ll"
	#define __PRIPTR_PREFIX
#endif

#define PRId8		"d"
#define PRId16		"d"
#define PRId32		"d"
#define PRId64		__PRI64_PREFIX "d"
#define PRIdLEAST8	"d"
#define PRIdLEAST16	"d"
#define PRIdLEAST32	"d"
#define PRIdLEAST64	__PRI64_PREFIX "d"
#define PRIdFAST8	"d"
#define PRIdFAST16	__PRIPTR_PREFIX "d"
#define PRIdFAST32	__PRIPTR_PREFIX "d"
#define PRIdFAST64	__PRI64_PREFIX "d"
#define PRIdMAX		__PRI64_PREFIX "d"
#define PRIdPTR		__PRIPTR_PREFIX "d"

#define PRIi8		"i"
#define PRIi16		"i"
#define PRIi32		"i"
#define PRIi64		__PRI64_PREFIX "i"
#define PRIiLEAST8	"i"
#define PRIiLEAST16	"i"
#define PRIiLEAST32	"i"
#define PRIiLEAST64	__PRI64_PREFIX "i"
#define PRIiFAST8	"i"
#define PRIiFAST16	__PRIPTR_PREFIX "i"
#define PRIiFAST32	__PRIPTR_PREFIX "i"
#define PRIiFAST64	__PRI64_PREFIX "i"
#define PRIiMAX		__PRI64_PREFIX "i"
#define PRIiPTR		__PRIPTR_PREFIX "i"

#define PRIo8		"o"
#define PRIo16		"o"
#define PRIo32		"o"
#define PRIo64		__PRI64_PREFIX "o"
#define PRIoLEAST8	"o"
#define PRIoLEAST16	"o"
#define PRIoLEAST32	"o"
#define PRIoLEAST64	__PRI64_PREFIX "o"
#define PRIoFAST8	"o"
#define PRIoFAST16	__PRIPTR_PREFIX "o"
#define PRIoFAST32	__PRIPTR_PREFIX "o"
#define PRIoFAST64	__PRI64_PREFIX "o"
#define PRIoMAX		__PRI64_PREFIX "o"
#define PRIoPTR		__PRIPTR_PREFIX "o"

#define PRIu8		"u"
#define PRIu16		"u"
#define PRIu32		"u"
#define PRIu64		__PRI64_PREFIX "u"
#define PRIuLEAST8	"u"
#define PRIuLEAST16	"u"
#define PRIuLEAST32	"u"
#define PRIuLEAST64	__PRI64_PREFIX "u"
#define PRIuFAST8	"u"
#define PRIuFAST16	__PRIPTR_PREFIX "u"
#define PRIuFAST32	__PRIPTR_PREFIX "u"
#define PRIuFAST64	__PRI64_PREFIX "u"
#define PRIuMAX		__PRI64_PREFIX "u"
#define PRIuPTR		__PRIPTR_PREFIX "u"

#define PRIx8		"x"
#define PRIx16		"x"
#define PRIx32		"x"
#define PRIx64		__PRI64_PREFIX "x"
#define PRIxLEAST8	"x"
#define PRIxLEAST16	"x"
#define PRIxLEAST32	"x"
#define PRIxLEAST64	__PRI64_PREFIX "x"
#define PRIxFAST8	"x"
#define PRIxFAST16	__PRIPTR_PREFIX "x"
#define PRIxFAST32	__PRIPTR_PREFIX "x"
#define PRIxFAST64	__PRI64_PREFIX "x"
#define PRIxMAX		__PRI64_PREFIX "x"
#define PRIxPTR		__PRIPTR_PREFIX "x"

#define PRIX8		"X"
#define PRIX16		"X"
#define PRIX32		"X"
#define PRIX64		__PRI64_PREFIX "X"
#define PRIXLEAST8	"X"
#define PRIXLEAST16	"X"
#define PRIXLEAST32	"X"
#define PRIXLEAST64	__PRI64_PREFIX "X"
#define PRIXFAST8	"X"
#define PRIXFAST16	__PRIPTR_PREFIX "X"
#define PRIXFAST32	__PRIPTR_PREFIX "X"
#define PRIXFAST64	__PRI64_PREFIX "X"
#define PRIXMAX		__PRI64_PREFIX "X"
#define PRIXPTR		__PRIPTR_PREFIX "X"

#define SCNd8		"hhd"
#define SCNd16		"hd"
#define SCNd32		"d"
#define SCNd64		__PRI64_PREFIX "d"
#define SCNdLEAST8	"hhd"
#define SCNdLEAST16	"hd"
#define SCNdLEAST32	"d"
#define SCNdLEAST64	__PRI64_PREFIX "d"
#define SCNdFAST8	"hhd"
#define SCNdFAST16	__PRIPTR_PREFIX "d"
#define SCNdFAST32	__PRIPTR_PREFIX "d"
#define SCNdFAST64	__PRI64_PREFIX "d"
#define SCNdMAX		__PRI64_PREFIX "d"
#define SCNdPTR		__PRIPTR_PREFIX "d"

#define SCNi8		"hhi"
#define SCNi16		"hi"
#define SCNi32		"i"
#define SCNi64		__PRI64_PREFIX "i"
#define SCNiLEAST8	"hhi"
#define SCNiLEAST16	"hi"
#define SCNiLEAST32	"i"
#define SCNiLEAST64	__PRI64_PREFIX "i"
#define SCNiFAST8	"hhi"
#define SCNiFAST16	__PRIPTR_PREFIX "i"
#define SCNiFAST32	__PRIPTR_PREFIX "i"
#define SCNiFAST64	__PRI64_PREFIX "i"
#define SCNiMAX		__PRI64_PREFIX "i"
#define SCNiPTR		__PRIPTR_PREFIX "i"

#define SCNo8		"hho"
#define SCNo16		"ho"
#define SCNo32		"o"
#define SCNo64		__PRI64_PREFIX "o"
#define SCNoLEAST8	"hho"
#define SCNoLEAST16	"ho"
#define SCNoLEAST32	"o"
#define SCNoLEAST64	__PRI64_PREFIX "o"
#define SCNoFAST8	"hho"
#define SCNoFAST16	__PRIPTR_PREFIX "o"
#define SCNoFAST32	__PRIPTR_PREFIX "o"
#define SCNoFAST64	__PRI64_PREFIX "o"
#define SCNoMAX		__PRI64_PREFIX "o"
#define SCNoPTR		__PRIPTR_PREFIX "o"

#define SCNu8		"hhu"
#define SCNu16		"hu"
#define SCNu32		"u"
#define SCNu64		__PRI64_PREFIX "u"
#define SCNuLEAST8	"hhu"
#define SCNuLEAST16	"hu"
#define SCNuLEAST32	"u"
#define SCNuLEAST64	__PRI64_PREFIX "u"
#define SCNuFAST8	"hhu"
#define SCNuFAST16	__PRIPTR_PREFIX "u"
#define SCNuFAST32	__PRIPTR_PREFIX "u"
#define SCNuFAST64	__PRI64_PREFIX "u"
#define SCNuMAX		__PRI64_PREFIX "u"
#define SCNuPTR		__PRIPTR_PREFIX "u"

#define SCNx8		"hhx"
#define SCNx16		"hx"
#define SCNx32		"x"
#define SCNx64		__PRI64_PREFIX "x"
#define SCNxLEAST8	"hhx"
#define SCNxLEAST16	"hx"
#define SCNxLEAST32	"x"
#define SCNxLEAST64	__PRI64_PREFIX "x"
#define SCNxFAST8	"hhx"
#define SCNxFAST16	__PRIPTR_PREFIX "x"
#define SCNxFAST32	__PRIPTR_PREFIX "x"
#define SCNxFAST64	__PRI64_PREFIX "x"
#define SCNxMAX		__PRI64_PREFIX "x"
#define SCNxPTR		__PRIPTR_PREFIX "x"

intmax_t	imaxabs(intmax_t);
imaxdiv_t	imaxdiv(intmax_t, intmax_t);
intmax_t	strtoimax(const char* __restrict nptr, char** __restrict endptr, int base);
uintmax_t	strtoumax(const char* __restrict nptr, char** __restrict endptr, int base);
intmax_t	wcstoimax(const wchar_t* __restrict nptr, wchar_t** __restrict endptr, int base);
uintmax_t	wcstoumax(const wchar_t* __restrict nptr, wchar_t** __restrict endptr, int base);

__END_DECLS

#endif
