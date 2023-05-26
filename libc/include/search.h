#ifndef _SEARCH_H
#define _SEARCH_H 1

// https://pubs.opengroup.org/onlinepubs/9699919799/basedefs/search.h.html

#include <sys/cdefs.h>

__BEGIN_DECLS

#define __need_size_t
#include <sys/types.h>

typedef struct
{
	char* key;
	void* data;
} ENTRY;

typedef enum { FIND, ENTER } ACTION;
typedef enum { preorder, postorder, endorder, leaf } VISIT;

int		hcreate(size_t nel);
void	hdestroy(void);
ENTRY*	hsearch(ENTRY item, ACTION action);
void	insque(void* element, void* pred);
void*	lfind(const void* key, const void* base, size_t* nelp, size_t width, int (*compar)(const void*, const void*));
void*	lsearch(const void* key, void* base, size_t* nelp, size_t width, int (*compar)(const void*, const void*));
void	remque(void* element);
void*	tdelete(const void* __restrict key, void** __restrict rootp, int(*compar)(const void*, const void*));
void*	tfind(const void* key, void* const* rootp, int(*compar)(const void*, const void*));
void*	tsearch(const void* key, void** rootp, int(*compar)(const void*, const void*));
void	twalk(const void* root, void (*action)(const void*, VISIT, int));

__END_DECLS

#endif
