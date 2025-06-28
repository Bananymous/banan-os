#ifndef _SYS_IOCTL_H
#define _SYS_IOCTL_H 1

#include <sys/cdefs.h>

__BEGIN_DECLS

#define I_ATMARK	1
#define I_CANPUT	2
#define I_CKBAND	3
#define I_FDINSERT	4
#define I_FIND		5
#define I_FLUSH		6
#define I_FLUSHBAND	7
#define I_GETBAND	8
#define I_GETCLTIME	9
#define I_GETSIG	10
#define I_GRDOPT	11
#define I_GWROPT	12
#define I_LINK		13
#define I_LIST		14
#define I_LOOK		15
#define I_NREAD		16
#define I_PEEK		17
#define I_PLINK		18
#define I_POP		19
#define I_PUNLINK	20
#define I_PUSH		21
#define I_RECVFD	22
#define I_SENDFD	23
#define I_SETCLTIME	24
#define I_SETSIG	25
#define I_SRDOPT	26
#define I_STR		27
#define I_SWROPT	28
#define I_UNLINK	29

#define KDLOADFONT	30

struct winsize
{
	unsigned short ws_row;
	unsigned short ws_col;
	unsigned short ws_xpixel; /* unused by kernel */
	unsigned short ws_ypixel; /* unused by kernel */
};
#define TIOCGWINSZ	50
#define TIOCSWINSZ	51

int ioctl(int, int, ...);

__END_DECLS

#endif
