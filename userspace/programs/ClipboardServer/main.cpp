#include <BAN/HashMap.h>
#include <BAN/Vector.h>

#include <LibClipboard/Clipboard.h>

#include <stdio.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <unistd.h>

static uid_t receive_credentials(int fd)
{
	char dummy = '\0';
	iovec iovec {
		.iov_base = &dummy,
		.iov_len = 1,
	};

	constexpr size_t control_size = CMSG_LEN(sizeof(ucred));
	uint8_t control_buffer[control_size];

	msghdr message {
		.msg_name = nullptr,
		.msg_namelen = 0,
		.msg_iov = &iovec,
		.msg_iovlen = 1,
		.msg_control = control_buffer,
		.msg_controllen = control_size,
		.msg_flags = 0,
	};

	const ssize_t nrecv = recvmsg(fd, &message, 0);
	if (nrecv <= 0)
	{
		if (nrecv < 0)
			dwarnln("recvmsg: {}", strerror(errno));
		return -1;
	}

	for (auto* cheader = CMSG_FIRSTHDR(&message); cheader; cheader = CMSG_NXTHDR(&message, cheader))
	{
		if (cheader->cmsg_level != SOL_SOCKET)
			continue;
		if (cheader->cmsg_type != SCM_CREDENTIALS)
			continue;

		auto* ucred = reinterpret_cast<const struct ucred*>(CMSG_DATA(cheader));
		if (ucred->uid < 0)
		{
			dwarnln("got uid {} from SCM_CREDENTIALS");
			return -1;
		}

		return ucred->uid;
	}

	dwarnln("no credentials in client's first message");
	return -1;
}

static bool recv_sized(int fd, void* data, size_t size)
{
	uint8_t* u8_data = static_cast<uint8_t*>(data);

	size_t total_recv = 0;
	while (total_recv < size)
	{
		const ssize_t nrecv = recv(fd, u8_data + total_recv, size - total_recv, 0);
		if (nrecv <= 0)
		{
			const int error = nrecv ? errno : ECONNRESET;
			dwarnln("recv: {}", strerror(error));
			return false;
		}
		total_recv += nrecv;
	}

	return true;
}

static bool send_sized(int fd, const void* data, size_t size)
{
	const uint8_t* u8_data = static_cast<const uint8_t*>(data);

	size_t total_sent = 0;
	while (total_sent < size)
	{
		const ssize_t nsend = send(fd, u8_data + total_sent, size - total_sent, 0);
		if (nsend <= 0)
		{
			const int error = nsend ? errno : ECONNRESET;
			dwarnln("send: {}", strerror(error));
			return false;
		}
		total_sent += nsend;
	}

	return true;
}

int main()
{
	using namespace LibClipboard;

	int server_sock = socket(AF_UNIX, SOCK_STREAM, 0);
	if (server_sock == -1)
	{
		dwarnln("socket: {}", strerror(errno));
		return 1;
	}

	sockaddr_un server_addr;
	server_addr.sun_family = AF_UNIX;
	strcpy(server_addr.sun_path, LibClipboard::s_clipboard_server_socket.data());
	if (bind(server_sock, reinterpret_cast<sockaddr*>(&server_addr), sizeof(server_addr)) == -1)
	{
		dwarnln("bind: {}", strerror(errno));
		return 1;
	}

	if (chmod(LibClipboard::s_clipboard_server_socket.data(), 0777) == -1)
		dwarnln("chmod: {}", strerror(errno));

	if (listen(server_sock, SOMAXCONN) == -1)
	{
		dwarnln("listen: {}", strerror(errno));
		return 1;
	}

	struct Client
	{
		int fd;
		uid_t uid = -1;
	};

	BAN::Vector<Client> clients;
	BAN::HashMap<uid_t, Clipboard::Info> clipboards;

	for (;;)
	{
		fd_set fds;
		FD_ZERO(&fds);

		int max_fd = server_sock;
		FD_SET(server_sock, &fds);

		for (const auto& client : clients)
		{
			FD_SET(client.fd, &fds);
			max_fd = BAN::Math::max(client.fd, max_fd);
		}

		if (select(max_fd + 1, &fds, nullptr, nullptr, nullptr) == -1)
			continue;

		if (FD_ISSET(server_sock, &fds))
		{
			const int client = accept(server_sock, nullptr, nullptr);
			if (client == -1)
				dwarnln("accept: {}", strerror(errno));
			else if (clients.emplace_back(client).is_error())
			{
				dwarnln("failed to allocate space for new client");
				close(client);
			}
		}

		for (size_t i = 0; i < clients.size(); i++)
		{
			auto& client = clients[i];
			if (!FD_ISSET(client.fd, &fds))
				continue;

			bool closed = false;

			if (client.uid == -1)
			{
				client.uid = receive_credentials(client.fd);
				if (client.uid == -1)
					closed = true;
				else if (!clipboards.contains(client.uid) && clipboards.emplace(client.uid).is_error())
				{
					dwarnln("failed to allocate clipboard for {}", client.fd);
					closed = true;
				}
			}
			else
			{
				Clipboard::DataType data_type;

				auto& clipboard = clipboards[client.uid];

				if (!recv_sized(client.fd, &data_type, sizeof(data_type)))
					closed = true;
				else switch (data_type)
				{
					case Clipboard::DataType::__get:
					{
						closed = true;

						const auto data_type = clipboard.type;
						if (!send_sized(client.fd, &data_type, sizeof(data_type)))
							break;

						const auto data_size = clipboard.data.size();
						if (!send_sized(client.fd, &data_size, sizeof(data_size)))
							break;

						if (!send_sized(client.fd, clipboard.data.data(), data_size))
							break;

						closed = false;
						break;
					}
					case Clipboard::DataType::None:
						clipboard = {
							.type = data_type,
							.data = {},
						};
						break;
					case Clipboard::DataType::Text:
					{
						closed = true;

						// FIXME: client can hang the server if it doesn't
						//        send the actual clipboard data...

						size_t data_size;
						if (!recv_sized(client.fd, &data_size, sizeof(data_size)))
							break;

						BAN::Vector<uint8_t> new_clipboard;
						if (new_clipboard.resize(data_size).is_error())
						{
							dwarnln("failed to allocate {} bytes for clipboard", data_size);
							break;
						}

						if (!recv_sized(client.fd, new_clipboard.data(), data_size))
							break;

						clipboard = {
							.type = data_type,
							.data = BAN::move(new_clipboard),
						};

						closed = false;
						break;
					}
					default:
						dwarnln("unexpected data type {}", static_cast<uint32_t>(data_type));
						closed = true;
						break;
				}
			}

			if (closed)
			{
				close(client.fd);
				clients.remove(i--);
			}
		}
	}
}
