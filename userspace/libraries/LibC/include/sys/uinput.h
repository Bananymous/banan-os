#ifndef _SYS_UINPUT_H
#define _SYS_UINPUT_H 1

#include <sys/cdefs.h>

__BEGIN_DECLS

#define UINPUT_DEVICE_TYPE_KEYBOARD 1
#define UINPUT_DEVICE_TYPE_MOUSE    2
#define UINPUT_DEVICE_TYPE_JOYSTICK 3

struct uinput_device
{
	unsigned type;
};

__END_DECLS

#endif
