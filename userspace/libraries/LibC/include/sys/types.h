#ifndef _SYS_TYPES_H

// https://pubs.opengroup.org/onlinepubs/9699919799/basedefs/sys_types.h.html

#include <sys/cdefs.h>

__BEGIN_DECLS

#if    !defined(__need_blkcnt_t) \
	&& !defined(__need_blksize_t) \
	&& !defined(__need_clock_t) \
	&& !defined(__need_clockid_t) \
	&& !defined(__need_dev_t) \
	&& !defined(__need_fsblkcnt_t) \
	&& !defined(__need_fsfilcnt_t) \
	&& !defined(__need_gid_t) \
	&& !defined(__need_id_t) \
	&& !defined(__need_ino_t) \
	&& !defined(__need_key_t) \
	&& !defined(__need_mode_t) \
	&& !defined(__need_nlink_t) \
	&& !defined(__need_off_t) \
	&& !defined(__need_pid_t) \
	&& !defined(__need_size_t) \
	&& !defined(__need_ssize_t) \
	&& !defined(__need_suseconds_t) \
	&& !defined(__need_time_t) \
	&& !defined(__need_timer_t) \
	&& !defined(__need_uid_t)

	#define __need_all_types
#endif

#ifdef __need_all_types
#define _SYS_TYPES_H 1
#endif

#if !defined(__blkcnt_t_defined) && (defined(__need_all_types) || defined(__need_blkcnt_t))
	#define __blkcnt_defined 1
	typedef long blkcnt_t;
#endif
#undef __need_blkcnt_t

#if !defined(__blksize_t_defined) && (defined(__need_all_types) || defined(__need_blksize_t))
	#define __blksize_t_defined 1
	typedef long blksize_t;
#endif
#undef __need_blksize_t

#if !defined(__clock_t_defined) && (defined(__need_all_types) || defined(__need_clock_t))
	#define __clock_t_defined 1
	typedef long clock_t;
#endif
#undef __need_clock_t

#if !defined(__clockid_t_defined) && (defined(__need_all_types) || defined(__need_clockid_t))
	#define __clockid_t_defined 1
	typedef int clockid_t;
#endif
#undef __need_clockid_t

#if !defined(__dev_t_defined) && (defined(__need_all_types) || defined(__need_dev_t))
	#define __dev_t_defined 1
	typedef unsigned int dev_t;
#endif
#undef __need_dev_t

#if !defined(__fsblkcnt_t_defined) && (defined(__need_all_types) || defined(__need_fsblkcnt_t))
	#define __fsblkcnt_t_defined 1
	typedef unsigned long fsblkcnt_t;
#endif
#undef __need_fsblkcnt_t

#if !defined(__fsfilcnt_t_defined) && (defined(__need_all_types) || defined(__need_fsfilcnt_t))
	#define __fsfilcnt_t_defined 1
	typedef unsigned long fsfilcnt_t;
#endif
#undef __need_fsfilcnt_t

#if !defined(__gid_t_defined) && (defined(__need_all_types) || defined(__need_gid_t))
	#define __gid_t_defined 1
	typedef int gid_t;
#endif
#undef __need_gid_t

#if !defined(__id_t_defined) && (defined(__need_all_types) || defined(__need_id_t))
	#define __id_t_defined 1
	typedef int id_t;
#endif
#undef __need_id_t

#if !defined(__ino_t_defined) && (defined(__need_all_types) || defined(__need_ino_t))
	#define __ino_t_defined 1
	typedef unsigned long long ino_t;
#endif
#undef __need_ino_t

#if !defined(__key_t_defined) && (defined(__need_all_types) || defined(__need_key_t))
	#define __key_t_defined 1
	typedef int key_t;
#endif
#undef __need_key_t

#if !defined(__mode_t_defined) && (defined(__need_all_types) || defined(__need_mode_t))
	#define __mode_t_defined 1
	typedef unsigned int mode_t;
#endif
#undef __need_mode_t

#if !defined(__nlink_t_defined) && (defined(__need_all_types) || defined(__need_nlink_t))
	#define __nlink_t_defined 1
	typedef unsigned long nlink_t;
#endif
#undef __need_nlink_t

#if !defined(__off_t_defined) && (defined(__need_all_types) || defined(__need_off_t))
	#define __off_t_defined 1
	typedef long off_t;
#endif
#undef __need_off_t

#if !defined(__pid_t_defined) && (defined(__need_all_types) || defined(__need_pid_t))
	#define __pid_t_defined 1
	typedef int pid_t;
#endif
#undef __need_pid_t

#if !defined(__size_t_defined) && (defined(__need_all_types) || defined(__need_size_t))
	#define __size_t_defined 1
	#define __need_size_t
	#include <stddef.h>
#endif
#undef __need_size_t

#if !defined(__ssize_t_defined) && (defined(__need_all_types) || defined(__need_ssize_t))
	#define __ssize_t_defined 1
	typedef __PTRDIFF_TYPE__ ssize_t;
#endif
#undef __need_ssize_t

#if !defined(__suseconds_t_defined) && (defined(__need_all_types) || defined(__need_suseconds_t))
	#define __suseconds_t_defined 1
	typedef long suseconds_t;
#endif
#undef __need_suseconds_t

#if !defined(__time_t_defined) && (defined(__need_all_types) || defined(__need_time_t))
	#define __time_t_defined 1
	typedef unsigned long long time_t;
#endif
#undef __need_time_t

#if !defined(__timer_t_defined) && (defined(__need_all_types) || defined(__need_timer_t))
	#define __timer_t_defined 1
	typedef void* timer_t;
#endif
#undef __need_timer_t

#if !defined(__uid_t_defined) && (defined(__need_all_types) || defined(__need_uid_t))
	#define __uid_t_defined 1
	typedef int uid_t;
#endif
#undef __need_uid_t

#if !defined(__useconds_t_defined) && (defined(__need_all_types) || defined(__need_useconds_t))
	#define __useconds_t_defined 1
	typedef unsigned long useconds_t;
#endif
#undef __need_useconds_t

#ifdef __need_all_types
	#include <stdint.h>

	typedef short bits16_t;
	typedef unsigned short u_bits16_t;
	typedef int bits32_t;
	typedef unsigned int u_bits32_t;
	typedef char* bits64_t;
	typedef unsigned char u_char;
	typedef unsigned short u_short;
	typedef unsigned int u_int;
	typedef unsigned long u_long;
#endif

#undef __need_all_types

__END_DECLS

#endif
