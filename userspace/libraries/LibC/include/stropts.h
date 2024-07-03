#ifndef _STROPTS_H
#define _STROPTS_H 1

// https://pubs.opengroup.org/onlinepubs/9699919799/basedefs/stropts.h.html

#include <sys/cdefs.h>

__BEGIN_DECLS

#define __need_uid_t
#define __need_gid_t
#include <sys/types.h>

typedef __UINT32_TYPE__ t_uscalar_t;
typedef __INT32_TYPE__ t_scalar_t;

struct bandinfo
{
	int				bi_flag;	/* Flushing type. */
	unsigned char	bi_pri;		/* Priority band. */
};

struct strbuf
{
	char*	buf;	/* Pointer to buffer. */
	int		len;	/* Length of data. */
	int		maxlen;	/* Maximum buffer length. */
};

struct strpeek
{
	struct strbuf	ctlbuf;		/* The control portion of the message. */
	struct strbuf	databuf;	/* The data portion of the message. */
	t_uscalar_t		flags;		/* RS_HIPRI or 0. */
};

struct strfdinsert
{
	struct strbuf	ctlbuf;		/* The control portion of the message. */
	struct strbuf	databuf;	/* The data portion of the message. */
	int				fildes;		/* File descriptor of the other STREAM. */
	t_uscalar_t		flags;		/* RS_HIPRI or 0. */
	int				offset;		/* Relative location of the stored value. */
};

struct strioctl
{
	int		ic_cmd;		/* ioctl() command. */
	char*	ic_dp;		/* Pointer to buffer. */
	int		ic_len;		/* Length of data. */
	int		ic_timout;	/* Timeout for response. */
};

struct strrecvfd
{
	int		fd;		/* Received file descriptor. */
	gid_t	gid;	/* GID of sender. */
	uid_t	uid;	/* UID of sender. */
};

#define FMNAMESZ 128

struct str_mlist
{
	char	l_name[FMNAMESZ+1];	/* A STREAMS module name. */
};

struct str_list
{
	struct str_mlist*	sl_modlist;	/* STREAMS module names. */
	int					sl_nmods;	/* Number of STREAMS module names. */
};

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

#define KD_LOADFONT	30

#define FLUSHR	1
#define FLUSHRW	2
#define FLUSHW	3

#define S_BANDURG	1
#define S_ERROR		2
#define S_HANGUP	3
#define S_HIPRI		4
#define S_INPUT		5
#define S_MSG		6
#define S_OUTPUT	7
#define S_RDBAND	8
#define S_RDNORM	9
#define S_WRBAND	10
#define S_WRNORM	11

#define RS_HIPRI 1

#define RMSGD		1
#define RMSGN		2
#define RNORM		3
#define RPROTDAT	4
#define RPROTDIS	5
#define RPROTNORM	6

#define SNDZERO 1

#define ANYMARK		1
#define LASTMARK	2

#define MUXID_ALL 1

#define MORECTL		1
#define MOREDATA	2
#define MSG_ANY		3
#define MSG_BAND	4
#define MSG_HIPRI	5

int fattach(int, const char*);
int fdetach(const char*);
int getmsg(int, struct strbuf* __restrict, struct strbuf* __restrict, int* __restrict);
int getpmsg(int, struct strbuf* __restrict, struct strbuf* __restrict, int* __restrict, int* __restrict);
int ioctl(int, int, ...);
int isastream(int);
int putmsg(int, const struct strbuf*, const struct strbuf*, int);
int putpmsg(int, const struct strbuf*, const struct strbuf*, int, int);

__END_DECLS

#endif
