#pragma once

#include <sys/cdefs.h>
#include <sys/types.h>

#define NCCS 0

// c_iflag
#define BRKINT	0x001
#define ICRNL	0x002
#define IGNBRK	0x004
#define IGNCR	0x008
#define IGNPAR	0x010
#define INLCR	0x020
#define INPCK	0x040
#define ISTRIP	0x080
#define IXANY	0x100
#define IXOFF	0x200
#define IXON	0x400
#define PARMRK	0x800

// c_oflag
#define OPOST	0x0001
#define ONLCR	0x0002
#define OCRNL	0x0004
#define ONOCR	0x0008
#define ONLRET	0x0010
#define OFDEL	0x0020
#define OFILL	0x0040
#define NLDLY	0x0080
	#define NL0 0
	#define NL1 1
#define CRDLY	0x0100
	#define CR0 0
	#define CR1 1
	#define CR2 2
	#define CR3 3
#define TABDLY	0x0200
	#define TAB0 0
	#define TAB1 1
	#define TAB2 2
	#define TAB3 3
#define BSDLY	0x0400
	#define BS0 0
	#define BS1 1
#define VTDLY	0x0800
	#define VT0 0
	#define VT1 1
#define FFDLY	0x1000
	#define FF0 0
	#define FF1 1

// c_cflag
#define CSIZE	0x01
	#define CS5 5
	#define CS6 6
	#define CS7 7
	#define CS8 8
#define CSTOPB	0x02
#define CREAD	0x04
#define PARENB	0x08
#define PARODD	0x10
#define HUPCL	0x20
#define CLOCAL	0x40

// c_lflag
#define ECHO	0x001
#define ECHOE	0x002
#define ECHOK	0x004
#define ECHONL	0x008
#define ICANON	0x010
#define IEXTEN	0x020
#define ISIG	0x040
#define NOFLSH	0x080
#define TOSTOP	0x100

// for tcsetattr
#define TCSANOW		1
#define TCSADRAIN	2
#define TCSAFLUSH	3

// for tcflush
#define TCIFLUSH	0x01
#define TCIOFLUSH	(TCIFLUSH | TCOFLUSH)
#define TCOFLUSH	0x02

// for tcflow
#define TCIOFF	1
#define TCION	2
#define TCOOFF	3
#define TCOON	4

__BEGIN_DECLS

typedef int cc_t;
typedef int speed_t;
typedef int tcflag_t;

struct termios
{
	tcflag_t c_iflag;
	tcflag_t c_oflag;
	tcflag_t c_cflag;
	tcflag_t c_lflag;
	cc_t c_cc[NCCS];
};

speed_t cfgetispeed(const struct termios*);
speed_t cfgetospeed(const struct termios*);
int		cfsetispeed(struct termios*, speed_t);
int		cfsetospeed(struct termios*, speed_t);
int		tcdrain(int);
int		tcflow(int, int);
int		tcflush(int, int);
int		tcgetattr(int, struct termios*);
pid_t	tcgetsid(int);
int		tcsendbreak(int, int);
int		tcsetattr(int, int, const struct termios*);

__END_DECLS
