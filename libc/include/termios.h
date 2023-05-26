#ifndef _TERMIOS_H
#define _TERMIOS_H 1

// https://pubs.opengroup.org/onlinepubs/9699919799/basedefs/termios.h.html

#include <sys/cdefs.h>

__BEGIN_DECLS

#define __need_pid_t
#include <sys/types.h>

typedef unsigned int cc_t;
typedef unsigned int speed_t;
typedef unsigned int tcflag_t;

#define VEOF	0
#define VEOL	1
#define VERASE	2
#define VINTR	3
#define VKILL	4
#define VMIN	5
#define VQUIT	6
#define VSTART	7
#define VSTOP	8
#define VSUSP	9
#define VTIME	10

#define NCCS	11

struct termios
{
	tcflag_t	c_iflag;	/* Input modes. */
	tcflag_t	c_oflag;	/* Output modes. */
	tcflag_t	c_cflag;	/* Control modes. */
	tcflag_t	c_lflag;	/* Local modes. */
	cc_t		c_cc[NCCS];	/* Control characters. */
};

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

#define OPOST		0x0001
#define ONLCR		0x0002
#define OCRNL		0x0004
#define ONOCR		0x0008
#define ONLRET		0x0010
#define OFDEL		0x0020
#define OFILL		0x0040
#define NLDLY		0x0080
	#define NL0		0x0000
	#define NL1		0x0080
#define CRDLY		0x0300
	#define CR0		0x0000
	#define CR1		0x0100
	#define CR2		0x0200
	#define CR3		0x0300
#define TABDLY		0x0C00
	#define TAB0	0x0000
	#define TAB1	0x0400
	#define TAB2	0x0800
	#define TAB3	0x0C00
#define BSDLY		0x1000
	#define BS0		0x0000
	#define BS1		0x1000
#define VTDLY		0x2000
	#define VT0		0x0000
	#define VT1		0x2000
#define FFDLY		0x4000
	#define FF0		0x0000
	#define FF1		0x4000

#define B0		0
#define B50		1
#define B75		2
#define B110	3
#define B134	4
#define B150	5
#define B200	6
#define B300	7
#define B600	8
#define B1200	9
#define B1800	10
#define B2400	11
#define B4800	12
#define B9600	13
#define B19200	14
#define B38400	15

#define CSIZE	0x03
	#define CS5 0x00
	#define CS6 0x01
	#define CS7 0x02
	#define CS8 0x03
#define CSTOPB	0x04
#define CREAD	0x08
#define PARENB	0x10
#define PARODD	0x20
#define HUPCL	0x40
#define CLOCAL	0x80

#define ECHO	0x001
#define ECHOE	0x002
#define ECHOK	0x004
#define ECHONL	0x008
#define ICANON	0x010
#define IEXTEN	0x020
#define ISIG	0x040
#define NOFLSH	0x080
#define TOSTOP	0x100

#define TCSANOW		0
#define TCSADRAIN	1
#define TCSAFLUSH	2

#define TCIFLUSH	0x01
#define TCOFLUSH	0x02
#define TCIOFLUSH	(TCIFLUSH | TCOFLUSH)

#define TCIOFF	0
#define TCION	1
#define TCOOFF	2
#define TCOON	3

speed_t	cfgetispeed(const struct termios* termios_p);
speed_t	cfgetospeed(const struct termios* termios_p);
int		cfsetispeed(struct termios* termios_p, speed_t speed);
int		cfsetospeed(struct termios* termios_p, speed_t speed);
int		tcdrain(int fildes);
int		tcflow(int fildes, int action);
int		tcflush(int fildes, int queue_selector);
int		tcgetattr(int fildes, struct termios* termios_p);
pid_t	tcgetsid(int fildes);
int		tcsendbreak(int fildes, int duration);
int		tcsetattr(int fildes, int optional_actions, const struct termios* termios_p);

__END_DECLS

#endif
