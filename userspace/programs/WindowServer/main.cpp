#include "WindowServer.h"

#include <BAN/Debug.h>
#include <BAN/ScopeGuard.h>

#include <LibGUI/Window.h>
#include <LibInput/KeyboardLayout.h>

#include <fcntl.h>
#include <limits.h>
#include <signal.h>
#include <stdlib.h>
#include <sys/banan-os.h>
#include <sys/epoll.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <time.h>
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

int g_epoll_fd = -1;

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

	g_epoll_fd = epoll_create1(EPOLL_CLOEXEC);
	if (g_epoll_fd == -1)
	{
		dwarnln("epoll_create1: {}", strerror(errno));
		return 1;
	}

	{
		epoll_event event {
			.events = EPOLLIN,
			.data = { .fd = server_fd },
		};
		if (epoll_ctl(g_epoll_fd, EPOLL_CTL_ADD, server_fd, &event) == -1)
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
		if (epoll_ctl(g_epoll_fd, EPOLL_CTL_ADD, keyboard_fd, &event) == -1)
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
		if (epoll_ctl(g_epoll_fd, EPOLL_CTL_ADD, mouse_fd, &event) == -1)
		{
			dwarnln("epoll_ctl mouse: {}", strerror(errno));
			close(mouse_fd);
			mouse_fd = -1;
		}
	}

	dprintln("Window server started");

	if (access("/usr/bin/xbanan", X_OK) == 0)
	{
		if (fork() == 0)
		{
			dup2(STDDBG_FILENO, STDOUT_FILENO);
			dup2(STDDBG_FILENO, STDERR_FILENO);
			execl("/usr/bin/xbanan", "xbanan", NULL);
			exit(1);
		}
	}

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
		timespec* ptimeout = nullptr;

		timespec timeout = {};
		if (window_server.is_damaged())
		{
			if (const auto current_us = get_current_us(); current_us - last_sync_us > sync_interval_us)
			{
				window_server.sync();

				const auto full_intervals = (current_us - last_sync_us) / sync_interval_us;
				last_sync_us += full_intervals * sync_interval_us;
			}

			if (const auto current_us = get_current_us(); current_us - last_sync_us < sync_interval_us)
				timeout.tv_nsec = (sync_interval_us - (current_us - last_sync_us)) * 1000;
			ptimeout = &timeout;
		}

		epoll_event events[16];
		int epoll_events = epoll_pwait2(g_epoll_fd, events, 16, ptimeout, nullptr);
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

				int client_fd = accept4(server_fd, nullptr, nullptr, SOCK_NONBLOCK | SOCK_CLOEXEC);
				if (client_fd == -1)
				{
					dwarnln("accept: {}", strerror(errno));
					continue;
				}

				epoll_event event { .events = EPOLLIN, .data = { .fd = client_fd } };
				if (epoll_ctl(g_epoll_fd, EPOLL_CTL_ADD, client_fd, &event) == -1)
				{
					dwarnln("epoll_ctl: {}", strerror(errno));
					close(client_fd);
					continue;
				}

				if (auto ret = window_server.add_client_fd(client_fd); ret.is_error())
				{
					dwarnln("add_client: {}", ret.error());
					close(client_fd);
					continue;
				}

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
			if (events[i].events & (EPOLLHUP | EPOLLERR))
			{
				epoll_ctl(g_epoll_fd, EPOLL_CTL_DEL, client_fd, nullptr);
				window_server.remove_client_fd(client_fd);
				continue;
			}

			auto& client_data = window_server.get_client_data(client_fd);

			if (events[i].events & EPOLLOUT)
			{
				ASSERT(client_data.out_buffer_size > 0);

				const ssize_t nsend = send(client_fd, client_data.out_buffer.data(), client_data.out_buffer_size, 0);
				if (nsend < 0 && !(errno == EWOULDBLOCK || errno == EAGAIN))
				{
					dwarnln("send: {}", strerror(errno));
					epoll_ctl(g_epoll_fd, EPOLL_CTL_DEL, client_fd, nullptr);
					window_server.remove_client_fd(client_fd);
					break;
				}

				if (nsend > 0)
				{
					client_data.out_buffer_size -= nsend;
					if (client_data.out_buffer_size == 0)
					{
						epoll_event event { .events = EPOLLIN, .data = { .fd = client_fd } };
						if (epoll_ctl(g_epoll_fd, EPOLL_CTL_MOD, client_fd, &event) == -1)
							dwarnln("epoll_ctl remove EPOLLOUT: {}", strerror(errno));
					}
					else
					{
						// TODO: maybe use a ring buffer so we don't have to memmove everything not sent
						memmove(
							client_data.out_buffer.data(),
							client_data.out_buffer.data() + nsend,
							client_data.out_buffer_size
						);
					}
				}
			}

			if (!(events[i].events & EPOLLIN))
				continue;

			{
				const ssize_t nrecv = recv(
					client_fd,
					client_data.in_buffer.data() + client_data.in_buffer_size,
					client_data.in_buffer.size() - client_data.in_buffer_size,
					0
				);
				if (nrecv < 0 && !(errno == EWOULDBLOCK || errno == EAGAIN))
				{
					dwarnln("recv: {}", strerror(errno));
					epoll_ctl(g_epoll_fd, EPOLL_CTL_DEL, client_fd, nullptr);
					window_server.remove_client_fd(client_fd);
					break;
				}
				if (nrecv > 0)
					client_data.in_buffer_size += nrecv;
			}

			size_t bytes_handled = 0;
			while (client_data.in_buffer_size - bytes_handled >= sizeof(LibGUI::PacketHeader))
			{
				BAN::ConstByteSpan packet_span = client_data.in_buffer.span().slice(bytes_handled, client_data.in_buffer_size - bytes_handled);
				const auto header = packet_span.as<const LibGUI::PacketHeader>();
				if (packet_span.size() < header.size || header.size < sizeof(LibGUI::PacketHeader))
					break;
				packet_span = packet_span.slice(0, header.size);

				switch (header.type)
				{
#define WINDOW_PACKET_CASE(enum, function) \
					case LibGUI::PacketType::enum: \
						if (auto ret = LibGUI::WindowPacket::enum::deserialize(packet_span); !ret.is_error()) \
							window_server.function(client_fd, ret.release_value()); \
						else \
							derrorln("invalid packet: {}", ret.error()); \
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
						dprintln("unhandled packet type: {}", static_cast<uint32_t>(header.type));
						break;
				}

				bytes_handled += header.size;
			}

			// NOTE: this will only move a single partial packet, so this is fine
			client_data.in_buffer_size -= bytes_handled;
			memmove(
				client_data.in_buffer.data(),
				client_data.in_buffer.data() + bytes_handled,
				client_data.in_buffer_size
			);

			if (client_data.in_buffer_size >= sizeof(LibGUI::PacketHeader))
			{
				const auto header = BAN::ConstByteSpan(client_data.in_buffer.span()).as<const LibGUI::PacketHeader>();
				if (header.size < sizeof(LibGUI::PacketHeader) || header.size > client_data.in_buffer.size())
				{
					dwarnln("client tried to send a {} byte packet", header.size);
					epoll_ctl(g_epoll_fd, EPOLL_CTL_DEL, client_fd, nullptr);
					window_server.remove_client_fd(client_fd);
					break;
				}
			}
		}
	}
}
