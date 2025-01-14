#ifndef _SYS_WEAK_ALIAS
#define _SYS_WEAK_ALIAS 1

#define weak_alias(name, aliasname) _weak_alias (name, aliasname)
#define _weak_alias(name, aliasname) extern __typeof (name) aliasname __attribute__ ((weak, alias (#name)))

#endif
