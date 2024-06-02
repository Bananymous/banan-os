#include "WindowServer.h"

#include <BAN/Debug.h>

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

int open_server_fd()
{
	struct stat st;
	if (stat(LibGUI::s_window_server_socket.data(), &st) != -1)
		unlink(LibGUI::s_window_server_socket.data());

	int server_fd = socket(AF_UNIX, SOCK_SEQPACKET, 0);
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

	if (chmod("/tmp/resolver.sock", 0777) == -1)
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

	MUST(LibInput::KeyboardLayout::initialize());
	MUST(LibInput::KeyboardLayout::get().load_from_file("/usr/share/keymaps/us.keymap"sv));

	int keyboard_fd = open("/dev/input0", O_RDONLY);
	if (keyboard_fd == -1)
		perror("open");

	int mouse_fd = open("/dev/input1", O_RDONLY);
	if (mouse_fd == -1)
		perror("open");

	dprintln("Window server started");

	size_t window_packet_sizes[LibGUI::WindowPacketType::COUNT] {};
	window_packet_sizes[LibGUI::WindowPacketType::INVALID]		= 0;
	window_packet_sizes[LibGUI::WindowPacketType::CreateWindow]	= sizeof(LibGUI::WindowCreatePacket);
	window_packet_sizes[LibGUI::WindowPacketType::Invalidate]	= sizeof(LibGUI::WindowInvalidatePacket);
	static_assert(LibGUI::WindowPacketType::COUNT == 3);

	WindowServer window_server(framebuffer);
	while (!window_server.is_stopped())
	{
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

		if (select(max_fd + 1, &fds, nullptr, nullptr, nullptr) == -1)
		{
			dwarnln("select: {}", strerror(errno));
			break;
		}

		if (FD_ISSET(server_fd, &fds))
		{
			int window_fd = accept(server_fd, nullptr, nullptr);
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
				case LibInput::MouseEventType::MouseScrollEvent:
					window_server.on_mouse_scroll(event.scroll_event);
					break;
			}
		}

		window_server.for_each_client_fd(
			[&](int fd) -> BAN::Iteration
			{
				if (!FD_ISSET(fd, &fds))
					return BAN::Iteration::Continue;

				LibGUI::WindowPacket packet;
				ssize_t nrecv = recv(fd, &packet, sizeof(packet), 0);
				if (nrecv < 0)
					dwarnln("recv: {}", strerror(errno));
				if (nrecv <= 0)
				{
					window_server.remove_client_fd(fd);
					return BAN::Iteration::Continue;
				}

				if (packet.type == LibGUI::WindowPacketType::INVALID || packet.type >= LibGUI::WindowPacketType::COUNT)
					dwarnln("Invalid WindowPacket (type {})", (int)packet.type);
				if (static_cast<size_t>(nrecv) != window_packet_sizes[packet.type])
					dwarnln("Invalid WindowPacket size (type {}, size {})", (int)packet.type, nrecv);
				else
					window_server.on_window_packet(fd, packet);
				return BAN::Iteration::Continue;
			}
		);
	}
}
