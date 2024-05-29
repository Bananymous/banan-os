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

	for (int i = 0; i < 2; i++)
	{
		if (fork() == 0)
		{
			execl("/bin/test-window", "test-window", NULL);
			exit(1);
		}
	}

	WindowServer window_server(framebuffer);
	for (;;)
	{
		int max_socket = server_fd;

		fd_set fds;
		FD_ZERO(&fds);
		FD_SET(server_fd, &fds);
		if (keyboard_fd != -1)
		{
			FD_SET(keyboard_fd, &fds);
			max_socket = BAN::Math::max(max_socket, keyboard_fd);
		}
		if (mouse_fd != -1)
		{
			FD_SET(mouse_fd, &fds);
			max_socket = BAN::Math::max(max_socket, mouse_fd);
		}
		window_server.for_each_window(
			[&](int fd, Window&) -> BAN::Iteration
			{
				FD_SET(fd, &fds);
				max_socket = BAN::Math::max(max_socket, fd);
				return BAN::Iteration::Continue;
			}
		);

		if (select(max_socket + 1, &fds, nullptr, nullptr, nullptr) == -1)
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
			auto window = MUST(BAN::RefPtr<Window>::create(window_fd));
			window_server.add_window(window_fd, window);
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

		window_server.for_each_window(
			[&](int fd, Window& window) -> BAN::Iteration
			{
				if (!FD_ISSET(fd, &fds))
					return BAN::Iteration::Continue;

				LibGUI::WindowPacket packet;
				ssize_t nrecv = recv(fd, &packet, sizeof(packet), 0);
				if (nrecv < 0)
					dwarnln("recv: {}", strerror(errno));
				if (nrecv <= 0)
				{
					window.mark_deleted();
					return BAN::Iteration::Continue;
				}

				switch (packet.type)
				{
					case LibGUI::WindowPacketType::CreateWindow:
					{
						if (nrecv != sizeof(LibGUI::WindowCreatePacket))
						{
							dwarnln("Invalid WindowCreate packet size");
							break;
						}

						const size_t window_fb_bytes = packet.create.width * packet.create.height * 4;

						long smo_key = smo_create(window_fb_bytes, PROT_READ | PROT_WRITE);
						if (smo_key == -1)
						{
							dwarnln("smo_create: {}", strerror(errno));
							break;
						}

						void* smo_address = smo_map(smo_key);
						if (smo_address == nullptr)
						{
							dwarnln("smo_map: {}", strerror(errno));
							break;
						}
						memset(smo_address, 0, window_fb_bytes);

						LibGUI::WindowCreateResponse response;
						response.framebuffer_smo_key = smo_key;
						if (send(fd, &response, sizeof(response), 0) != sizeof(response))
						{
							dwarnln("send: {}", strerror(errno));
							break;
						}

						window.set_size({
							static_cast<int32_t>(packet.create.width),
							static_cast<int32_t>(packet.create.height)
						}, reinterpret_cast<uint32_t*>(smo_address));
						window.set_position({
							static_cast<int32_t>(window.width() / 2),
							static_cast<int32_t>(window.height() / 2)
						});

						break;
					}
					case LibGUI::WindowPacketType::Invalidate:
					{
						if (nrecv != sizeof(LibGUI::WindowInvalidatePacket))
						{
							dwarnln("Invalid Invalidate packet size");
							break;
						}
						if (packet.invalidate.x + packet.invalidate.width > window.width() || packet.invalidate.y + packet.invalidate.height > window.height())
						{
							dwarnln("Invalid Invalidate packet parameters");
							break;
						}

						window_server.invalidate({
							window.x() + static_cast<int32_t>(packet.invalidate.x),
							window.y() + static_cast<int32_t>(packet.invalidate.y),
							static_cast<int32_t>(packet.invalidate.width),
							static_cast<int32_t>(packet.invalidate.height),
						});

						break;
					}
					default:
						dwarnln("Invalid window packet from {}", fd);
				}

				return BAN::Iteration::Continue;
			}
		);
	}
}
