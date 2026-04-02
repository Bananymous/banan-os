#include <BAN/Optional.h>

#include <LibAudio/Protocol.h>

#include <errno.h>
#include <getopt.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

static uint32_t parse_u32_or_exit(const char* string)
{
	errno = 0;
	char* endptr;
	const uint32_t result = strtoul(string, &endptr, 0);
	if (errno || *endptr != '\0')
	{
		fprintf(stderr, "invalid integer %s\n", string);
		exit(1);
	}
	return result;
}

static int get_server_fd()
{
	int fd = socket(AF_UNIX, SOCK_STREAM, 0);
	if (fd == -1)
	{
		perror("Failed to create a socket");
		return -1;
	}

	sockaddr_un addr;
	addr.sun_family = AF_UNIX;
	strcpy(addr.sun_path, LibAudio::s_audio_server_socket.data());
	if (connect(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == -1)
	{
		perror("Failed to connect to audio server");
		return -1;
	}

	return fd;
}

static uint32_t send_request(int fd, LibAudio::Packet packet, bool wait_response)
{
	if (ssize_t ret = send(fd, &packet, sizeof(packet), 0); ret != sizeof(packet))
	{
		fprintf(stderr, "Failed to send request to server");
		if (ret < 0)
			fprintf(stderr, ": %s", strerror(errno));
		fprintf(stderr, "\n");
		exit(1);
	}

	if (!wait_response)
		return 0;

	uint32_t response;
	if (ssize_t ret = recv(fd, &response, sizeof(response), 0) != sizeof(response))
	{
		fprintf(stderr, "Failed to receive response from server");
		if (ret < 0)
			fprintf(stderr, ": %s", strerror(errno));
		fprintf(stderr, "\n");
		exit(1);
	}

	return response;
}

static void list_devices(int fd)
{
	const uint32_t current_device = send_request(fd, { .type = LibAudio::Packet::GetDevice,    .parameter = {} }, true);
	const uint32_t current_pin    = send_request(fd, { .type = LibAudio::Packet::GetPin,       .parameter = {} }, true);

	const uint32_t total_devices  = send_request(fd, { .type = LibAudio::Packet::QueryDevices, .parameter = {} }, true);
	for (uint32_t dev = 0; dev < total_devices; dev++)
	{
		const uint32_t total_pins = send_request(fd, { .type = LibAudio::Packet::QueryPins, .parameter = dev }, true);

		printf("Device %" PRIu32 "", dev);
		if (dev == current_device)
		{
			const uint32_t volume = send_request(fd, { .type = LibAudio::Packet::GetVolume, .parameter = {} }, true);
			printf(" (current) %" PRIu32 "%%", volume / 10);
		}
		printf("\n");

		for (uint32_t pin = 0; pin < total_pins; pin++)
		{
			printf("  Pin %" PRIu32 "", pin);
			if (dev == current_device && pin == current_pin)
				printf(" (current)");
			printf("\n");
		}
	}
}

int main(int argc, char** argv)
{
	bool list { false };
	BAN::Optional<uint32_t> device;
	BAN::Optional<uint32_t> pin;

	BAN::Optional<uint32_t> volume;
	BAN::Optional<bool> volume_rel; // no value: absolute, false positive, true negative

	for (;;)
	{
		static option long_options[] {
			{ "list",   no_argument,       nullptr, 'l' },
			{ "device", required_argument, nullptr, 'd' },
			{ "pin",    required_argument, nullptr, 'p' },
			{ "volume", required_argument, nullptr, 'v' },
			{ "help",   no_argument,       nullptr, 'h' },
		};

		int ch = getopt_long(argc, argv, "ld:p:v:h", long_options, nullptr);
		if (ch == -1)
			break;

		switch (ch)
		{
			case 'h':
				fprintf(stderr, "usage: %s [OPTIONS]...\n", argv[0]);
				fprintf(stderr, "  control the audio server\n");
				fprintf(stderr, "OPTIONS:\n");
				fprintf(stderr, "  -l, --list      list devices and their pins\n");
				fprintf(stderr, "  -d, --device N  set device index N as the current one\n");
				fprintf(stderr, "  -p, --pin N     set pin N as the current one\n");
				fprintf(stderr, "  -v, --volume N  set volume to N%%. if + or - is given, volume is relative to the current volume\n");
				fprintf(stderr, "  -h, --help      show this message and exit\n");
				return 0;
			case 'l':
				list = true;
				break;
			case 'd':
				device = parse_u32_or_exit(optarg);
				break;
			case 'p':
				pin = parse_u32_or_exit(optarg);
				break;
			case 'v':
				if (optarg[0] == '-' || optarg[0] == '+')
					volume_rel = (optarg[0] == '-');
				volume = parse_u32_or_exit(optarg + volume_rel.has_value());
				break;
			case '?':
				fprintf(stderr, "invalid option %c\n", optopt);
				fprintf(stderr, "see '%s --help' for usage\n", argv[0]);
				return 1;
		}
	}

	if (!device.has_value() && !pin.has_value() && !volume.has_value())
		list = true;

	const int fd = get_server_fd();
	if (fd == -1)
		return 1;

	if (device.has_value())
		send_request(fd, { .type = LibAudio::Packet::SetDevice, .parameter = device.value() }, false);

	if (pin.has_value())
		send_request(fd, { .type = LibAudio::Packet::SetPin, .parameter = pin.value() }, false);

	if (volume.has_value())
	{
		const uint32_t volume10 = volume.value() * 10;

		uint32_t param;
		if (!volume_rel.has_value())
			param = volume10;
		else
		{
			const uint32_t current = send_request(fd, { .type = LibAudio::Packet::GetVolume, .parameter = {} }, true);
			if (!volume_rel.value())
				param = current + volume10;
			else
			{
				if (current > volume10)
					param = current - volume10;
				else
					param = 0;
			}
		}

		send_request(fd, { .type = LibAudio::Packet::SetVolume, .parameter = param }, false);
	}

	if (list)
		list_devices(fd);

	close(fd);

	return 0;
}
