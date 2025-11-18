#include <LibClipboard/Clipboard.h>

#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

namespace LibClipboard
{

	static int s_server_fd = -1;

	static BAN::ErrorOr<void> send_credentials(int fd)
	{
		char dummy = '\0';
		iovec iovec {
			.iov_base = &dummy,
			.iov_len = 1,
		};

		constexpr size_t control_size = CMSG_LEN(sizeof(ucred));
		uint8_t control_buffer[control_size];

		cmsghdr* control = reinterpret_cast<cmsghdr*>(control_buffer);

		*control = {
			.cmsg_len = control_size,
			.cmsg_level = SOL_SOCKET,
			.cmsg_type = SCM_CREDENTIALS,
		};

		*reinterpret_cast<ucred*>(CMSG_DATA(control)) = {
			.pid = getpid(),
			.uid = getuid(),
			.gid = getgid(),
		};

		const msghdr message {
			.msg_name = nullptr,
			.msg_namelen = 0,
			.msg_iov = &iovec,
			.msg_iovlen = 1,
			.msg_control = control,
			.msg_controllen = control_size,
			.msg_flags = 0,
		};

		if (sendmsg(fd, &message, 0) < 0)
			return BAN::Error::from_errno(errno);

		return {};
	}

	static BAN::ErrorOr<void> ensure_connected()
	{
		if (s_server_fd != -1)
			return {};

		const int sock = socket(AF_UNIX, SOCK_STREAM, 0);
		if (sock == -1)
			return BAN::Error::from_errno(errno);

		sockaddr_un server_addr;
		server_addr.sun_family = AF_UNIX;
		strcpy(server_addr.sun_path, s_clipboard_server_socket.data());

		if (connect(sock, reinterpret_cast<const sockaddr*>(&server_addr), sizeof(server_addr)) == -1)
		{
			close(sock);
			return BAN::Error::from_errno(errno);
		}

		if (auto ret = send_credentials(sock); ret.is_error())
		{
			close(sock);
			return ret;
		}

		s_server_fd = sock;
		return {};
	}

	static BAN::ErrorOr<void> recv_sized(void* data, size_t size)
	{
		ASSERT(s_server_fd != -1);

		uint8_t* u8_data = static_cast<uint8_t*>(data);

		size_t total_recv = 0;
		while (total_recv < size)
		{
			const ssize_t nrecv = recv(s_server_fd, u8_data + total_recv, size - total_recv, 0);
			if (nrecv <= 0)
			{
				const int error = nrecv ? errno : ECONNRESET;
				close(s_server_fd);
				s_server_fd = -1;
				return BAN::Error::from_errno(error);
			}
			total_recv += nrecv;
		}

		return {};
	}

	static BAN::ErrorOr<void> send_sized(const void* data, size_t size)
	{
		ASSERT(s_server_fd != -1);

		const uint8_t* u8_data = static_cast<const uint8_t*>(data);

		size_t total_sent = 0;
		while (total_sent < size)
		{
			const ssize_t nsend = send(s_server_fd, u8_data + total_sent, size - total_sent, 0);
			if (nsend <= 0)
			{
				const int error = nsend ? errno : ECONNRESET;
				close(s_server_fd);
				s_server_fd = -1;
				return BAN::Error::from_errno(error);
			}
			total_sent += nsend;
		}

		return {};
	}

	BAN::ErrorOr<Clipboard::Info> Clipboard::get_clipboard()
	{
		TRY(ensure_connected());

		{
			DataType type = DataType::__get;
			TRY(send_sized(&type, sizeof(type)));
		}

		Info info;
		TRY(recv_sized(&info.type, sizeof(info.type)));

		switch (info.type)
		{
			case DataType::__get:
				ASSERT_NOT_REACHED();
			case DataType::None:
				break;
			case DataType::Text:
				size_t data_size;
				TRY(recv_sized(&data_size, sizeof(data_size)));

				TRY(info.data.resize(data_size));
				TRY(recv_sized(info.data.data(), data_size));
				break;
		}

		return info;
	}

	BAN::ErrorOr<void> Clipboard::set_clipboard(DataType type, BAN::Span<const uint8_t> data)
	{
		ASSERT(type != DataType::__get);

		TRY(ensure_connected());

		TRY(send_sized(&type, sizeof(type)));

		switch (type)
		{
			case DataType::__get:
				ASSERT_NOT_REACHED();
			case DataType::None:
				break;
			case DataType::Text:
				const size_t size = data.size();
				TRY(send_sized(&size, sizeof(size)));
				TRY(send_sized(data.data(), size));
				break;
		}

		return {};
	}

	BAN::ErrorOr<BAN::String> Clipboard::get_clipboard_text()
	{
		auto info = TRY(get_clipboard());
		if (info.type != DataType::Text)
			return BAN::String {};

		BAN::String string;
		TRY(string.resize(info.data.size()));
		memcpy(string.data(), info.data.data(), info.data.size());

		return string;
	}

	BAN::ErrorOr<void> Clipboard::set_clipboard_text(BAN::StringView string)
	{
		return set_clipboard(DataType::Text, { reinterpret_cast<const uint8_t*>(string.data()), string.size() });
	}

}
