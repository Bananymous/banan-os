#ifndef _NDBM_H
#define _NDBM_H 1

// https://pubs.opengroup.org/onlinepubs/9699919799/basedefs/ndbm.h.html

#include <sys/cdefs.h>

__BEGIN_DECLS

#define DBM_REPLACE 1
#define DBM_INSERT 0

#define __need_size_t
#include <stddef.h>

#define __need_mode_t
#include <sys/types.h>

typedef struct
{
	void* dptr;		/* A pointer to the application's data. */
	size_t dsize;	/* The size of the object pointed to by dptr */
} datum;

typedef int DBM;

int		dbm_clearerr(DBM* db);
void	dbm_close(DBM* db);
int		dbm_delete(DBM* db, datum key);
int		dbm_error(DBM* db);
datum	dbm_fetch(DBM* db, datum key);
datum	dbm_firstkey(DBM* db);
datum	dbm_nextkey(DBM* db);
DBM*	dbm_open(const char* file, int open_flags, mode_t file_mode);
int		dbm_store(DBM* db, datum key, datum content, int store_mode);

__END_DECLS

#endif
