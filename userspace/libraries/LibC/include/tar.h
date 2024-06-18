#ifndef _TAR_H
#define _TAR_H 1

// https://pubs.opengroup.org/onlinepubs/9699919799/basedefs/tar.h.html

#include <sys/cdefs.h>

__BEGIN_DECLS

#define TMAGIC		"ustar"
#define TMAGLEN		6
#define TVERSION	"00"
#define TVERSLEN	2

#define REGTYPE		'0'
#define AREGTYPE	'\0'
#define LNKTYPE		'1'
#define SYMTYPE		'2'
#define CHRTYPE		'3'
#define BLKTYPE		'4'
#define DIRTYPE		'5'
#define FIFOTYPE	'6'
#define CONTTYPE	'7'

#define TSUID	04000
#define TSGID	02000
#define TSVTX	01000
#define TUREAD	00400
#define TUWRITE	00200
#define TUEXEC	00100
#define TGREAD	00040
#define TGWRITE	00020
#define TGEXEC	00010
#define TOREAD	00004
#define TOWRITE	00002
#define TOEXEC	00001

__END_DECLS

#endif
