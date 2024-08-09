#include <BAN/Assert.h>
#include <BAN/Debug.h>

#include <errno.h>
#include <sys/syscall.h>
#include <termios.h>
#include <unistd.h>

speed_t cfgetispeed(const struct termios* termios)
{
	return termios->c_ispeed;
}

speed_t cfgetospeed(const struct termios* termios)
{
	return termios->c_ospeed;
}

static bool is_valid_speed(speed_t speed)
{
	switch (speed)
	{
		case B0:
		case B50:
		case B75:
		case B110:
		case B134:
		case B150:
		case B200:
		case B300:
		case B600:
		case B1200:
		case B1800:
		case B2400:
		case B4800:
		case B9600:
		case B19200:
		case B38400:
			return true;
		default:
			return false;
	}
}

int cfsetispeed(struct termios* termios, speed_t speed)
{
	if (!is_valid_speed(speed))
	{
		errno = EINVAL;
		return -1;
	}
	termios->c_ispeed = speed;
	return 0;
}

int cfsetospeed(struct termios* termios, speed_t speed)
{
	if (!is_valid_speed(speed))
	{
		errno = EINVAL;
		return -1;
	}
	termios->c_ospeed = speed;
	return 0;
}

int tcdrain(int);

int tcflow(int, int);

int tcflush(int fd, int queue_selector)
{
	dwarnln("FIXME: tcflush({}, {})", fd, queue_selector);
	return 0;
}

int tcgetattr(int fildes, struct termios* termios)
{
	return syscall(SYS_TCGETATTR, fildes, termios);
}

pid_t tcgetsid(int);

int tcsendbreak(int, int);

int tcsetattr(int fildes, int optional_actions, const struct termios* termios)
{
	return syscall(SYS_TCSETATTR, fildes, optional_actions, termios);
}
