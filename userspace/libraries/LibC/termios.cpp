#include <BAN/Assert.h>
#include <BAN/Debug.h>

#include <errno.h>
#include <pthread.h>
#include <sys/ioctl.h>
#include <sys/syscall.h>
#include <termios.h>
#include <unistd.h>

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

speed_t cfgetispeed(const struct termios* termios)
{
	return termios->c_ispeed;
}

speed_t cfgetospeed(const struct termios* termios)
{
	return termios->c_ospeed;
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

int tcdrain(int fd)
{
	pthread_testcancel();

	dwarnln("TODO: tcdrain({})", fd);
	return 0;
}

int tcflow(int fd, int action)
{
	dwarnln("TODO: tcflow({}, {})", fd, action);
	return -1;
}

int tcflush(int fd, int queue_selector)
{
	dwarnln("FIXME: tcflush({}, {})", fd, queue_selector);
	return 0;
}

int tcgetattr(int fildes, struct termios* termios)
{
	return ioctl(fildes, TCGETS, termios);
}

int tcsetattr(int fildes, int optional_actions, const struct termios* termios)
{
	int ioctl_num;
	switch (optional_actions)
	{
		case TCSANOW:
			ioctl_num = TCSETS;
			break;
		case TCSADRAIN:
			ioctl_num = TCSETSW;
			break;
		case TCSAFLUSH:
			ioctl_num = TCSETSF;
			break;
		default:
			errno = EINVAL;
			return -1;
	}
	return ioctl(fildes, ioctl_num, termios);
}

int tcsendbreak(int fd, int duration)
{
	dwarnln("FIXME: tcsendbreak({}, {})", fd, duration);
	return -1;
}
