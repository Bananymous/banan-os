#ifndef _SYS_CDEFS_H
#define _SYS_CDEFS_H 1

#define __banan_libc 1

#ifdef __cplusplus
	#define __BEGIN_DECLS extern "C" {
	#define __END_DECLS }
#else
	#define __BEGIN_DECLS
	#define __END_DECLS
#endif

#endif