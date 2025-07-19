#include "WindowServer.h"

#include <BAN/Debug.h>
#include <BAN/ScopeGuard.h>

#include <LibGUI/Window.h>
#include <LibInput/KeyboardLayout.h>

#include <fcntl.h>
#include <stdlib.h>
#include <sys/banan-os.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/select.h>
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

	if (tty_ctrl(STDIN_FILENO, TTY_CMD_UNSET, TTY_FLAG_ENABLE_INPUT) == -1)
	{
		perror("tty_ctrl");
		exit(1);
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
		perror("open");

	int mouse_fd = open("/dev/mouse", O_RDONLY | O_CLOEXEC);
	if (mouse_fd == -1)
		perror("open");

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
			return (current_ts.tv_sec * 1'000'000) + (current_ts.tv_nsec / 1'000);
		};

	constexpr uint64_t sync_interval_us = 1'000'000 / 60;
	uint64_t last_sync_us = 0;
	while (!window_server.is_stopped())
	{
		const auto current_us = get_current_us();
		if (current_us - last_sync_us > sync_interval_us)
		{
			window_server.sync();
			last_sync_us += sync_interval_us;
		}

		int max_fd = server_fd;

		fd_set fds;
		FD_ZERO(&fds);
		FD_SET(server_fd, &fds);
		if (keyboard_fd != -1)
		{
			FD_SET(keyboard_fd, &fds);
			max_fd = BAN::Math::max(max_fd, keyboard_fd);
		}
		if (mouse_fd != -1)
		{
			FD_SET(mouse_fd, &fds);
			max_fd = BAN::Math::max(max_fd, mouse_fd);
		}
		max_fd = BAN::Math::max(max_fd, window_server.get_client_fds(fds));

		timeval select_timeout {};
		if (auto current_us = get_current_us(); current_us - last_sync_us < sync_interval_us)
			select_timeout.tv_usec = sync_interval_us - (current_us - last_sync_us);

		int nselect = select(max_fd + 1, &fds, nullptr, nullptr, &select_timeout);
		if (nselect == 0)
			continue;
		if (nselect == -1)
		{
			dwarnln("select: {}", strerror(errno));
			break;
		}

		if (FD_ISSET(server_fd, &fds))
		{
			int window_fd = accept4(server_fd, nullptr, nullptr, SOCK_CLOEXEC);
			if (window_fd == -1)
			{
				dwarnln("accept: {}", strerror(errno));
				continue;
			}
			window_server.add_client_fd(window_fd);
		}

		if (keyboard_fd != -1 && FD_ISSET(keyboard_fd, &fds))
		{
			LibInput::RawKeyEvent event;
			if (read(keyboard_fd, &event, sizeof(event)) == -1)
			{
				perror("read");
				continue;
			}
			window_server.on_key_event(LibInput::KeyboardLayout::get().key_event_from_raw(event));
		}

		if (mouse_fd != -1 && FD_ISSET(mouse_fd, &fds))
		{
			LibInput::MouseEvent event;
			if (read(mouse_fd, &event, sizeof(event)) == -1)
			{
				perror("read");
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
		}

		window_server.for_each_client_fd(
			[&](int fd, WindowServer::ClientData& client_data) -> BAN::Iteration
			{
				if (!FD_ISSET(fd, &fds))
					return BAN::Iteration::Continue;

				if (client_data.packet_buffer.empty())
				{
					uint32_t packet_size;
					const ssize_t nrecv = recv(fd, &packet_size, sizeof(uint32_t), 0);
					if (nrecv < 0)
						dwarnln("recv: {}", strerror(errno));
					if (nrecv > 0 && nrecv != sizeof(uint32_t))
						dwarnln("could not read packet size with a single recv call, closing connection...");
					if (nrecv != sizeof(uint32_t))
					{
						window_server.remove_client_fd(fd);
						return BAN::Iteration::Continue;
					}

					if (packet_size < 4)
					{
						dwarnln("client sent invalid packet, closing connection...");
						return BAN::Iteration::Continue;
					}

					// this is a bit harsh, but i don't want to work on skipping streaming packets
					if (client_data.packet_buffer.resize(packet_size).is_error())
					{
						dwarnln("could not allocate memory for client packet, closing connection...");
						window_server.remove_client_fd(fd);
						return BAN::Iteration::Continue;
					}

					client_data.packet_buffer_nread = 0;
					return BAN::Iteration::Continue;
				}

				const ssize_t nrecv = recv(
					fd,
					client_data.packet_buffer.data() + client_data.packet_buffer_nread,
					client_data.packet_buffer.size() - client_data.packet_buffer_nread,
					0
				);
				if (nrecv < 0)
					dwarnln("recv: {}", strerror(errno));
				if (nrecv <= 0)
				{
					window_server.remove_client_fd(fd);
					return BAN::Iteration::Continue;
				}

				client_data.packet_buffer_nread += nrecv;
				if (client_data.packet_buffer_nread < client_data.packet_buffer.size())
					return BAN::Iteration::Continue;

				ASSERT(client_data.packet_buffer.size() >= sizeof(uint32_t));

				switch (*reinterpret_cast<LibGUI::PacketType*>(client_data.packet_buffer.data()))
				{
#define WINDOW_PACKET_CASE(enum, function) \
					case LibGUI::PacketType::enum: \
						if (auto ret = LibGUI::WindowPacket::enum::deserialize(client_data.packet_buffer.span()); !ret.is_error()) \
							window_server.function(fd, ret.release_value()); \
						break
					WINDOW_PACKET_CASE(WindowCreate,          on_window_create);
					WINDOW_PACKET_CASE(WindowInvalidate,      on_window_invalidate);
					WINDOW_PACKET_CASE(WindowSetPosition,     on_window_set_position);
					WINDOW_PACKET_CASE(WindowSetAttributes,   on_window_set_attributes);
					WINDOW_PACKET_CASE(WindowSetMouseCapture, on_window_set_mouse_capture);
					WINDOW_PACKET_CASE(WindowSetSize,         on_window_set_size);
					WINDOW_PACKET_CASE(WindowSetMinSize,      on_window_set_min_size);
					WINDOW_PACKET_CASE(WindowSetMaxSize,      on_window_set_max_size);
					WINDOW_PACKET_CASE(WindowSetFullscreen,   on_window_set_fullscreen);
					WINDOW_PACKET_CASE(WindowSetTitle,        on_window_set_title);
					WINDOW_PACKET_CASE(WindowSetCursor,       on_window_set_cursor);
#undef WINDOW_PACKET_CASE
					default:
						dprintln("unhandled packet type: {}", *reinterpret_cast<uint32_t*>(client_data.packet_buffer.data()));
				}

				client_data.packet_buffer.clear();
				client_data.packet_buffer_nread = 0;
				return BAN::Iteration::Continue;
			}
		);
	}
}
