#include "AudioServer.h"

#include <LibAudio/Audio.h>

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/epoll.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <unistd.h>

static int open_server_fd()
{
	struct stat st;
	if (stat(LibAudio::s_audio_server_socket.data(), &st) != -1)
		unlink(LibAudio::s_audio_server_socket.data());

	int server_fd = socket(AF_UNIX, SOCK_STREAM, 0);
	if (server_fd == -1)
	{
		dwarnln("failed to create server socket: {}", strerror(errno));
		return -1;
	}

	sockaddr_un server_addr;
	server_addr.sun_family = AF_UNIX;
	strcpy(server_addr.sun_path, LibAudio::s_audio_server_socket.data());
	if (bind(server_fd, (sockaddr*)&server_addr, sizeof(server_addr)) == -1)
	{
		dwarnln("failed to bind server socket: {}", strerror(errno));
		return -1;
	}

	if (chmod(LibAudio::s_audio_server_socket.data(), 0777) == -1)
	{
		dwarnln("failed to set server socket permissions: {}", strerror(errno));
		return -1;
	}

	if (listen(server_fd, SOMAXCONN) == -1)
	{
		dwarnln("failed to make server socket listening: {}", strerror(errno));
		return -1;
	}

	return server_fd;
}

static uint64_t get_current_ms()
{
	timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

static BAN::Optional<AudioDevice> initialize_audio_device(int fd)
{
	AudioDevice result {};
	result.fd = fd;
	if (ioctl(fd, SND_GET_CHANNELS, &result.channels) != 0)
		return {};
	if (ioctl(fd, SND_GET_SAMPLE_RATE, &result.sample_rate) != 0)
		return {};
	if (ioctl(fd, SND_GET_TOTAL_PINS, &result.total_pins) != 0)
		return {};
	if (ioctl(fd, SND_GET_PIN, &result.current_pin) != 0)
		return {};
	return result;
}

int main()
{
	constexpr int non_terminating_signals[] {
		SIGCONT,
		SIGSTOP,
		SIGTSTP,
		SIGTTIN,
		SIGTTOU,
	};
	for (int sig = _SIGMIN; sig <= _SIGMAX; sig++)
		signal(sig, exit);
	for (int sig : non_terminating_signals)
		signal(sig, SIG_DFL);

	BAN::Vector<AudioDevice> audio_devices;
	for (int i = 0; i < 16; i++)
	{
		char path[PATH_MAX];
		sprintf(path, "/dev/audio%d", i);

		const int fd = open(path, O_RDWR | O_NONBLOCK);
		if (fd == -1)
			continue;

		auto device = initialize_audio_device(fd);
		if (!device.has_value())
			close(fd);
		else
			MUST(audio_devices.push_back(device.release_value()));
	}

	if (audio_devices.empty())
	{
		dwarnln("could not open any audio device");
		return 1;
	}

	auto* audio_server = new AudioServer(BAN::move(audio_devices));
	if (audio_server == nullptr)
	{
		dwarnln("Failed to allocate AudioServer: {}", strerror(errno));
		return 1;
	}

	const int server_fd = open_server_fd();
	if (server_fd == -1)
		return 1;

	const int epoll_fd = epoll_create1(0);
	if (epoll_fd == -1)
	{
		dwarnln("failed to create epoll: {}", strerror(errno));
		return 1;
	}

	{
		epoll_event event {
			.events = EPOLLIN,
			.data = { .fd = server_fd },
		};
		if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, server_fd, &event) == -1)
		{
			dwarnln("failed to add server socket to epoll: {}", strerror(errno));
			return -1;
		}
	}

	dprintln("AudioServer started");

	uint64_t next_update_ms = get_current_ms();

	for (;;)
	{
		const uint64_t current_ms = get_current_ms();
		next_update_ms = current_ms + audio_server->update();

		const uint64_t timeout_ms = next_update_ms - current_ms;

		epoll_event events[16];
		int event_count = epoll_wait(epoll_fd, events, 16, timeout_ms);
		if (event_count == -1 && errno != EINTR)
		{
			dwarnln("epoll_wait: {}", strerror(errno));
			break;
		}

		for (int i = 0; i < event_count; i++)
		{
			if (events[i].data.fd == server_fd)
			{
				ASSERT(events[i].events & EPOLLIN);

				const int client_fd = accept(server_fd, nullptr, nullptr);
				if (client_fd == -1)
				{
					dwarnln("accept: {}", strerror(errno));
					continue;
				}

				epoll_event event {
					.events = EPOLLIN,
					.data = { .fd = client_fd },
				};
				if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, client_fd, &event) == -1)
				{
					dwarnln("Failed to add client to epoll: {}", strerror(errno));
					close(client_fd);
					continue;
				}

				if (auto ret = audio_server->on_new_client(client_fd); ret.is_error())
				{
					dwarnln("Failed to initialize client: {}", ret.error());
					epoll_ctl(epoll_fd, EPOLL_CTL_DEL, client_fd, nullptr);
					close(client_fd);
					continue;
				}

				continue;
			}

			const int client_fd = events[i].data.fd;

			if (events[i].events & EPOLLHUP)
			{
				audio_server->on_client_disconnect(client_fd);
				epoll_ctl(epoll_fd, EPOLL_CTL_DEL, client_fd, nullptr);
				close(client_fd);
				continue;
			}

			ASSERT(events[i].events & EPOLLIN);

			LibAudio::Packet packet;
			const ssize_t nrecv = recv(client_fd, &packet, sizeof(packet), 0);

			if (nrecv < static_cast<ssize_t>(sizeof(packet)) || !audio_server->on_client_packet(client_fd, packet))
			{
				if (nrecv == 0)
					;
				else if (nrecv < 0)
					dwarnln("recv: {}", strerror(errno));
				else if (nrecv < static_cast<ssize_t>(sizeof(packet)))
					dwarnln("client sent only {} bytes, {} expected", nrecv, sizeof(packet));

				audio_server->on_client_disconnect(client_fd);
				epoll_ctl(epoll_fd, EPOLL_CTL_DEL, client_fd, nullptr);
				close(client_fd);
			}
		}
	}
}
