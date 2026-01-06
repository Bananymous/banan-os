#include "AudioServer.h"

#include <LibAudio/Audio.h>

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <sys/select.h>
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

	struct ClientInfo
	{
		int fd;
	};

	BAN::Vector<ClientInfo> clients;

	dprintln("AudioServer started");

	uint64_t next_update_ms = get_current_ms();

	for (;;)
	{
		fd_set fds;
		FD_ZERO(&fds);
		FD_SET(server_fd, &fds);

		int max_fd = server_fd;
		for (const auto& client : clients)
		{
			max_fd = BAN::Math::max(max_fd, client.fd);
			FD_SET(client.fd, &fds);
		}

		const uint64_t current_ms = get_current_ms();
		next_update_ms = current_ms + audio_server->update();

		const uint64_t timeout_ms = next_update_ms - current_ms;

		timeval timeout {
			.tv_sec = static_cast<time_t>(timeout_ms / 1000),
			.tv_usec = static_cast<suseconds_t>((timeout_ms % 1000) * 1000)
		};
		if (select(max_fd + 1, &fds, nullptr, nullptr, &timeout) == -1)
		{
			dwarnln("select: {}", strerror(errno));
			break;
		}

		if (FD_ISSET(server_fd, &fds))
		{
			const int client_fd = accept(server_fd, nullptr, nullptr);
			if (client_fd == -1)
			{
				dwarnln("accept: {}", strerror(errno));
				continue;
			}

			if (auto ret = clients.emplace_back(client_fd); ret.is_error())
			{
				dwarnln("Failed to add client: {}", ret.error());
				close(client_fd);
				continue;
			}

			if (auto ret = audio_server->on_new_client(client_fd); ret.is_error())
			{
				dwarnln("Failed to initialize client: {}", ret.error());
				clients.pop_back();
				close(client_fd);
				continue;
			}
		}

		for (size_t i = 0; i < clients.size(); i++)
		{
			auto& client = clients[i];
			if (!FD_ISSET(client.fd, &fds))
				continue;

			LibAudio::Packet packet;
			const ssize_t nrecv = recv(client.fd, &packet, sizeof(packet), 0);

			if (nrecv < static_cast<ssize_t>(sizeof(packet)) || !audio_server->on_client_packet(client.fd, packet))
			{
				if (nrecv == 0)
					;
				else if (nrecv < 0)
					dwarnln("recv: {}", strerror(errno));
				else if (nrecv < static_cast<ssize_t>(sizeof(packet)))
					dwarnln("client sent only {} bytes, {} expected", nrecv, sizeof(packet));

				audio_server->on_client_disconnect(client.fd);
				close(client.fd);
				clients.remove(i--);
				continue;
			}
		}
	}
}
