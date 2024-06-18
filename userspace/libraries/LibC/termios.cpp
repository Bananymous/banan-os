#include <sys/syscall.h>
#include <termios.h>
#include <unistd.h>

speed_t cfgetispeed(const struct termios*);

speed_t cfgetospeed(const struct termios*);

int cfsetispeed(struct termios*, speed_t);

int cfsetospeed(struct termios*, speed_t);

int tcdrain(int);

int tcflow(int, int);

int tcflush(int, int);

int tcgetattr(int, struct termios* termios)
{
	return syscall(SYS_GET_TERMIOS, termios);
}

pid_t tcgetsid(int);

int tcsendbreak(int, int);

int tcsetattr(int, int, const struct termios* termios)
{
	return syscall(SYS_SET_TERMIOS, termios);
}
