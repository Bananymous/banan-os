#include <stdio.h>
#include <sys/ioctl.h>
#include <sys/uinput.h>
#include <fcntl.h>

#include <LibInput/KeyEvent.h>

int main(int argc, char** argv)
{
	int ufd = open("/dev/uinput", O_WRONLY);
	if (ufd == -1) {
		perror("open /dev/uinput");
		return 1;
	}

	const uinput_device device {
		.type = UINPUT_DEVICE_TYPE_KEYBOARD
	};

	int kbfd = ioctl(ufd, UINPUT_DEVICE_CREATE, &device);
	if (kbfd == -1) {
		perror("ioctl");
		return 1;
	}

	using Mod = LibInput::KeyEvent::Modifier;

	constexpr LibInput::RawKeyEvent events[] {
		{ Mod::LShift, 102 },
		{           0,  67 },
		{           0, 105 },
		{           0, 105 },
		{           0,  73 },
		{           0, 163 },
		{ Mod::LShift,  66 },
		{           0,  73 },
		{           0,  68 },
		{           0, 105 },
		{           0,  99 },
		{           0, 109 },
	};

	for (auto event : events)
	{
		event.modifier |=  Mod::Pressed;
		write(kbfd, &event, sizeof(event));
		event.modifier &= ~Mod::Pressed;
		write(kbfd, &event, sizeof(event));
	}

	fgetc(stdin);
}
