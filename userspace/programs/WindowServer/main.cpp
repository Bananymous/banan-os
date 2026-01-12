#include "WindowServer.h"

#include <BAN/Debug.h>
#include <BAN/ScopeGuard.h>

#include <LibGUI/Window.h>
#include <LibInput/KeyboardLayout.h>

#include <fcntl.h>
#include <stdlib.h>
#include <sys/banan-os.h>
#include <sys/epoll.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <unistd.h>

struct Config
{
	BAN::UniqPtr<LibImage::Image> background_image;
	int32_t corner_radius = 0;
};

BAN::Optional<BAN::String> file_read_line(FILE* file)
{
	BAN::String line;

	char buffer[128];
	while (fgets(buffer, sizeof(buffer), file))
	{
		MUST(line.append(buffer));
		if (line.back() == '\n')
		{
			line.pop_back();
			return BAN::move(line);
		}
	}

	if (line.empty())
		return {};
	return BAN::move(line);
}

Config parse_config()
{
	Config config;

	auto home_env = getenv("HOME");
	if (!home_env)
	{
		dprintln("HOME environment variable not set");
		return config;
	}

	auto config_path = MUST(BAN::String::formatted("{}/.config/WindowServer.conf", home_env));
	FILE* fconfig = fopen(config_path.data(), "r");
	if (!fconfig)
	{
		dprintln("Could not open '{}'", config_path);
		return config;
	}

	BAN::ScopeGuard _([fconfig] { fclose(fconfig); });

	while (true)
	{
		auto line = file_read_line(fconfig);
		if (!line.has_value())
			break;
		if (line->empty())
			continue;

		auto parts = MUST(line->sv().split('='));
		if (parts.size() != 2)
		{
			dwarnln("Invalid config line: {}", line.value());
			break;
		}

		auto variable = parts[0];
		auto value = parts[1];

		if (variable == "bg"_sv)
		{
			auto image = LibImage::Image::load_from_file(value);
			if (image.is_error())
				dwarnln("Could not load image: {}", image.error());
			else
				config.background_image = image.release_value();
		}
		else if (variable == "corner-radius"_sv)
		{
			char* endptr = nullptr;
			long long corner_radius = strtoll(value.data(), &endptr, 0);
			if (corner_radius < 0 || corner_radius == LONG_MAX || corner_radius >= INT32_MAX)
				dwarnln("invalid corner-radius: '{}'", value);
			else
				config.corner_radius = corner_radius;
		}
		else
		{
			dwarnln("Unknown config variable: {}", variable);
			break;
		}
	}

	return config;
}

int open_server_fd()
{
	struct stat st;
	if (stat(LibGUI::s_window_server_socket.data(), &st) != -1)
		unlink(LibGUI::s_window_server_socket.data());

	int server_fd = socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
	if (server_fd == -1)
	{
		perror("socket");
		exit(1);
	}

	sockaddr_un server_addr;
	server_addr.sun_family = AF_UNIX;
	strcpy(server_addr.sun_path, LibGUI::s_window_server_socket.data());
	if (bind(server_fd, (sockaddr*)&server_addr, sizeof(server_addr)) == -1)
	{
		perror("bind");
		exit(1);
	}

	if (chmod(LibGUI::s_window_server_socket.data(), 0777) == -1)
	{
		perror("chmod");
		exit(1);
	}

	if (listen(server_fd, SOMAXCONN) == -1)
	{
		perror("listen");
		exit(1);
	}

	return server_fd;
}

int main()
{
	srand(time(nullptr));

	int server_fd = open_server_fd();
	auto framebuffer = open_framebuffer();
	if (framebuffer.bpp != 32)
	{
		dwarnln("Window server requires 32 bpp");
		return 1;
	}

	int epoll_fd = epoll_create1(EPOLL_CLOEXEC);
	if (epoll_fd == -1)
	{
		dwarnln("epoll_create1: {}", strerror(errno));
		return 1;
	}

	{
		epoll_event event {
			.events = EPOLLIN,
			.data = { .fd = server_fd },
		};
		if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, server_fd, &event) == -1)
		{
			dwarnln("epoll_ctl server: {}", strerror(errno));
			return 1;
		}
	}

	if (tty_ctrl(STDIN_FILENO, TTY_CMD_UNSET, TTY_FLAG_ENABLE_INPUT) == -1)
	{
		dwarnln("tty_ctrl: {}", strerror(errno));
		return 1;
	}

	atexit([]() { tty_ctrl(STDIN_FILENO, TTY_CMD_SET, TTY_FLAG_ENABLE_INPUT); });

	constexpr int non_terminating_signals[] {
		SIGCHLD,
		SIGCONT,
		SIGSTOP,
		SIGTSTP,
		SIGTTIN,
		SIGTTOU,
	};
	constexpr int ignored_signals[] {
		SIGPIPE,
	};
	for (int sig = _SIGMIN; sig <= _SIGMAX; sig++)
		signal(sig, exit);
	for (int sig : non_terminating_signals)
		signal(sig, SIG_DFL);
	for (int sig : ignored_signals)
		signal(sig, SIG_IGN);

	MUST(LibInput::KeyboardLayout::initialize());
	MUST(LibInput::KeyboardLayout::get().load_from_file("/usr/share/keymaps/us.keymap"_sv));

	int keyboard_fd = open("/dev/keyboard", O_RDONLY | O_CLOEXEC);
	if (keyboard_fd == -1)
		dwarnln("open keyboard: {}", strerror(errno));
	else
	{
		epoll_event event {
			.events = EPOLLIN,
			.data = { .fd = keyboard_fd },
		};
		if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, keyboard_fd, &event) == -1)
		{
			dwarnln("epoll_ctl keyboard: {}", strerror(errno));
			close(keyboard_fd);
			keyboard_fd = -1;
		}
	}

	int mouse_fd = open("/dev/mouse", O_RDONLY | O_CLOEXEC);
	if (mouse_fd == -1)
		dwarnln("open mouse: {}", strerror(errno));
	else
	{
		epoll_event event {
			.events = EPOLLIN,
			.data = { .fd = mouse_fd },
		};
		if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, mouse_fd, &event) == -1)
		{
			dwarnln("epoll_ctl mouse: {}", strerror(errno));
			close(mouse_fd);
			mouse_fd = -1;
		}
	}

	dprintln("Window server started");

	auto config = parse_config();

	WindowServer window_server(framebuffer, config.corner_radius);
	if (config.background_image)
		if (auto ret = window_server.set_background_image(BAN::move(config.background_image)); ret.is_error())
			dwarnln("Could not set background image: {}", ret.error());

	const auto get_current_us =
		[]() -> uint64_t
		{
			timespec current_ts;
			clock_gettime(CLOCK_MONOTONIC, &current_ts);
			return (current_ts.tv_sec * 1'000'000) + (current_ts.tv_nsec / 1000);
		};

	constexpr uint64_t sync_interval_us = 1'000'000 / 60;
	uint64_t last_sync_us = get_current_us() - sync_interval_us;
	while (!window_server.is_stopped())
	{
		const auto current_us = get_current_us();
		if (current_us - last_sync_us > sync_interval_us)
		{
			window_server.sync();
			last_sync_us += sync_interval_us;
		}

		timespec timeout = {};
		if (auto current_us = get_current_us(); current_us - last_sync_us < sync_interval_us)
			timeout.tv_nsec = (sync_interval_us - (current_us - last_sync_us)) * 1000;

		epoll_event events[16];
		int epoll_events = epoll_pwait2(epoll_fd, events, 16, &timeout, nullptr);
		if (epoll_events == -1 && errno != EINTR)
		{
			dwarnln("epoll_pwait2: {}", strerror(errno));
			break;
		}

		for (int i = 0; i < epoll_events; i++)
		{
			if (events[i].data.fd == server_fd)
			{
				ASSERT(events[i].events & EPOLLIN);

				int window_fd = accept4(server_fd, nullptr, nullptr, SOCK_NONBLOCK | SOCK_CLOEXEC);
				if (window_fd == -1)
				{
					dwarnln("accept: {}", strerror(errno));
					continue;
				}

				epoll_event event {
					.events = EPOLLIN,
					.data = { .fd = window_fd },
				};
				if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, window_fd, &event) == -1)
				{
					dwarnln("epoll_ctl: {}", strerror(errno));
					close(window_fd);
					continue;
				}

				window_server.add_client_fd(window_fd);
				continue;
			}

			if (events[i].data.fd == keyboard_fd)
			{
				ASSERT(events[i].events & EPOLLIN);

				LibInput::RawKeyEvent event;
				if (read(keyboard_fd, &event, sizeof(event)) == -1)
				{
					dwarnln("read keyboard: {}", strerror(errno));
					continue;
				}
				window_server.on_key_event(LibInput::KeyboardLayout::get().key_event_from_raw(event));
				continue;
			}

			if (events[i].data.fd == mouse_fd)
			{
				ASSERT(events[i].events & EPOLLIN);

				LibInput::MouseEvent event;
				if (read(mouse_fd, &event, sizeof(event)) == -1)
				{
					dwarnln("read mouse: {}", strerror(errno));
					continue;
				}
				switch (event.type)
				{
					case LibInput::MouseEventType::MouseButtonEvent:
						window_server.on_mouse_button(event.button_event);
						break;
					case LibInput::MouseEventType::MouseMoveEvent:
						window_server.on_mouse_move(event.move_event);
						break;
					case LibInput::MouseEventType::MouseMoveAbsEvent:
						window_server.on_mouse_move_abs(event.move_abs_event);
						break;
					case LibInput::MouseEventType::MouseScrollEvent:
						window_server.on_mouse_scroll(event.scroll_event);
						break;
				}
				continue;
			}

			const int client_fd = events[i].data.fd;
			if (events[i].events & EPOLLHUP)
			{
				epoll_ctl(epoll_fd, EPOLL_CTL_DEL, client_fd, nullptr);
				window_server.remove_client_fd(client_fd);
				continue;
			}

			ASSERT(events[i].events & EPOLLIN);

			auto& client_data = window_server.get_client_data(client_fd);

			if (client_data.packet_buffer.empty())
			{
				uint32_t packet_size;
				const ssize_t nrecv = recv(client_fd, &packet_size, sizeof(uint32_t), 0);
				if (nrecv < 0)
					dwarnln("recv 1: {}", strerror(errno));
				if (nrecv > 0 && nrecv != sizeof(uint32_t))
					dwarnln("could not read packet size with a single recv call, closing connection...");
				if (nrecv != sizeof(uint32_t))
				{
					epoll_ctl(epoll_fd, EPOLL_CTL_DEL, client_fd, nullptr);
					window_server.remove_client_fd(client_fd);
					break;
				}

				if (packet_size < 4)
				{
					dwarnln("client sent invalid packet, closing connection...");
					epoll_ctl(epoll_fd, EPOLL_CTL_DEL, client_fd, nullptr);
					window_server.remove_client_fd(client_fd);
					break;
				}

				// this is a bit harsh, but i don't want to work on skipping streaming packets
				if (client_data.packet_buffer.resize(packet_size).is_error())
				{
					dwarnln("could not allocate memory for client packet, closing connection...");
					epoll_ctl(epoll_fd, EPOLL_CTL_DEL, client_fd, nullptr);
					window_server.remove_client_fd(client_fd);
					break;
				}

				client_data.packet_buffer_nread = 0;
				continue;
			}

			const ssize_t nrecv = recv(
				client_fd,
				client_data.packet_buffer.data() + client_data.packet_buffer_nread,
				client_data.packet_buffer.size() - client_data.packet_buffer_nread,
				0
			);
			if (nrecv < 0)
				dwarnln("recv 2: {}", strerror(errno));
			if (nrecv <= 0)
			{
				epoll_ctl(epoll_fd, EPOLL_CTL_DEL, client_fd, nullptr);
				window_server.remove_client_fd(client_fd);
				break;
			}

			client_data.packet_buffer_nread += nrecv;
			if (client_data.packet_buffer_nread < client_data.packet_buffer.size())
				continue;

			ASSERT(client_data.packet_buffer.size() >= sizeof(uint32_t));

			switch (*reinterpret_cast<LibGUI::PacketType*>(client_data.packet_buffer.data()))
			{
#define WINDOW_PACKET_CASE(enum, function) \
				case LibGUI::PacketType::enum: \
					if (auto ret = LibGUI::WindowPacket::enum::deserialize(client_data.packet_buffer.span()); !ret.is_error()) \
						window_server.function(client_fd, ret.release_value()); \
					break
				WINDOW_PACKET_CASE(WindowCreate,           on_window_create);
				WINDOW_PACKET_CASE(WindowInvalidate,       on_window_invalidate);
				WINDOW_PACKET_CASE(WindowSetPosition,      on_window_set_position);
				WINDOW_PACKET_CASE(WindowSetAttributes,    on_window_set_attributes);
				WINDOW_PACKET_CASE(WindowSetMouseRelative, on_window_set_mouse_relative);
				WINDOW_PACKET_CASE(WindowSetSize,          on_window_set_size);
				WINDOW_PACKET_CASE(WindowSetMinSize,       on_window_set_min_size);
				WINDOW_PACKET_CASE(WindowSetMaxSize,       on_window_set_max_size);
				WINDOW_PACKET_CASE(WindowSetFullscreen,    on_window_set_fullscreen);
				WINDOW_PACKET_CASE(WindowSetTitle,         on_window_set_title);
				WINDOW_PACKET_CASE(WindowSetCursor,        on_window_set_cursor);
#undef WINDOW_PACKET_CASE
				default:
					dprintln("unhandled packet type: {}", *reinterpret_cast<uint32_t*>(client_data.packet_buffer.data()));
			}

			client_data.packet_buffer.clear();
			client_data.packet_buffer_nread = 0;
		}
	}
}
