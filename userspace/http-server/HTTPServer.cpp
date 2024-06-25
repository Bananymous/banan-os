#include "HTTPServer.h"

#include <BAN/Debug.h>
#include <BAN/ScopeGuard.h>

#include <arpa/inet.h>
#include <ctype.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <sys/socket.h>

static BAN::StringView status_to_brief(unsigned);
static BAN::StringView extension_to_mime(BAN::StringView);

HTTPServer::HTTPServer() = default;

HTTPServer::~HTTPServer()
{
	if (m_listen_socket != -1)
		close(m_listen_socket);
}

BAN::ErrorOr<void> HTTPServer::initialize(BAN::StringView root, BAN::IPv4Address ip, int port)
{
	{
		char path_buffer[PATH_MAX];
		if (root.size() >= PATH_MAX)
			return BAN::Error::from_errno(ENAMETOOLONG);
		strcpy(path_buffer, root.data());

		char canonical_buffer[PATH_MAX];
		if (realpath(path_buffer, canonical_buffer) == NULL)
			return BAN::Error::from_errno(errno);

		TRY(m_web_root.append(canonical_buffer));
	}

	sockaddr_in addr;
	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);
	addr.sin_addr.s_addr = ip.raw;

	int sock = socket(AF_INET, SOCK_STREAM, 0);
	if (sock == -1)
		return BAN::Error::from_errno(errno);
	BAN::ScopeGuard socket_guard([sock] { close(sock); });

	if (bind(sock, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == -1)
		return BAN::Error::from_errno(errno);

	if (listen(sock, SOMAXCONN) == -1)
		return BAN::Error::from_errno(errno);

	m_listen_socket = sock;

	socket_guard.disable();
	return {};
}

void HTTPServer::start()
{
	ASSERT(m_listen_socket != -1);

	while (true)
	{
		fd_set fds;
		FD_ZERO(&fds);

		FD_SET(m_listen_socket, &fds);
		int max_fd = m_listen_socket;

		for (const auto& [fd, _] : m_client_data)
		{
			FD_SET(fd, &fds);
			max_fd = BAN::Math::max(max_fd, fd);
		}

		if (select(max_fd + 1, &fds, nullptr, nullptr, nullptr) == -1)
		{
			perror("select");
			break;
		}

		if (FD_ISSET(m_listen_socket, &fds))
		{
			int new_fd = accept(m_listen_socket, nullptr, nullptr);
			if (new_fd == -1)
			{
				perror("accept");
				continue;
			}
			MUST(m_client_data.emplace(new_fd));
		}

		for (auto& [fd, data] : m_client_data)
		{
			if (!FD_ISSET(fd, &fds))
				continue;

			char buffer[1024];
			int nrecv = recv(fd, buffer, sizeof(buffer), 0);
			if (nrecv < 0)
				perror("recv");
			if (nrecv <= 0)
			{
				close(fd);
				m_client_data.remove(fd);
				break;
			}

			size_t old_size = data.size();
			if (data.resize(old_size + nrecv).is_error())
			{
				close(fd);
				m_client_data.remove(fd);
				break;
			}

			memcpy(data.data() + old_size, buffer, nrecv);

			if (!handle_all_requests(fd, data))
			{
				close(fd);
				m_client_data.remove(fd);
				break;
			}
		}
	}
}

BAN::ErrorOr<HTTPRequest> HTTPServer::get_http_request(BAN::Vector<uint8_t>& data_vec)
{
	auto data = BAN::ConstByteSpan(data_vec.span());

	if (data.size() < 4)
		return BAN::Error::from_errno(ENODATA);
	size_t len = 0;
	for (;; len++)
	{
		if (len > data.size() - 4)
			return BAN::Error::from_errno(ENODATA);
		if (!isprint(data[len]) && !isspace(data[len]))
			return BAN::Error::from_errno(EINVAL);
		if (data[len + 0] != '\r')
			continue;
		if (data[len + 1] != '\n')
			continue;
		if (data[len + 2] != '\r')
			continue;
		if (data[len + 3] != '\n')
			continue;
		break;
	}

	auto header_data = BAN::StringView(reinterpret_cast<const char*>(data.data()), len + 1);

	auto lines = TRY(header_data.split('\n', false));
	if (lines.empty())
		return BAN::Error::from_errno(EINVAL);
	for (auto& line : lines)
	{
		if (line.empty() || line.back() != '\r')
			return BAN::Error::from_errno(EINVAL);
		line = line.substring(0, line.size() - 1);
	}

	HTTPRequest request;

	{
		auto request_line = TRY(lines[0].split(' '));
		if (request_line.size() != 3)
			return BAN::Error::from_errno(EINVAL);
		request.method = request_line[0];
		request.path = request_line[1];
		request.version = request_line[2];
	}

	size_t content_length = 0;
	for (size_t i = 1; i < lines.size(); i++)
	{
		auto opt_colon = lines[i].find(':');
		if (!opt_colon.has_value())
			return BAN::Error::from_errno(EINVAL);

		auto name = lines[i].substring(0, opt_colon.value());

		auto value = lines[i].substring(opt_colon.value() + 1);
		while (!value.empty() && isspace(value.front()))
			value = value.substring(1);
		while (!value.empty() && isspace(value.back()))
			value = value.substring(0, value.size() - 1);

		TRY(request.headers.emplace_back(name, value));

		if (name.size() == "Content-Length"_sv.size())
		{
			bool is_content_length = true;
			for (size_t i = 0; i < name.size() && is_content_length; i++)
				if (tolower(name[i]) != tolower("Content-Length"_sv[i]))
					is_content_length = false;
			if (is_content_length)
				content_length = strtoul(value.data(), nullptr, 10);
		}
	}

	if (data.size() < len + 4 + content_length)
		return BAN::Error::from_errno(ENODATA);

	request.body = data.slice(len + 4, content_length);

	{
		size_t request_size = len + 4 + content_length;
		size_t new_size = data.size() - request_size;
		memmove(data_vec.data(), data_vec.data() + request_size, new_size);
		MUST(data_vec.resize(new_size));
	}

	return request;
}

BAN::ErrorOr<void> HTTPServer::send_http_response(int fd, unsigned status, BAN::ConstByteSpan data, BAN::StringView mime)
{
	dprintln("HTTP/1.1 {} {}", status, status_to_brief(status));

	BAN::String output;
	TRY(output.append(MUST(BAN::String::formatted("HTTP/1.1 {} {}\r\n", status, status_to_brief(status)))));
	if (!mime.empty())
		TRY(output.append(MUST(BAN::String::formatted("Content-Type: {}\r\n", mime))));
	TRY(output.append(MUST(BAN::String::formatted("Content-Length: {}\r\n", data.size()))));
	TRY(output.append("\r\n"));

	size_t total_sent = 0;
	while (total_sent < output.size())
	{
		ssize_t nsend = send(fd, output.data() + total_sent, output.size() - total_sent, 0);
		if (nsend == -1)
			return BAN::Error::from_errno(errno);
		if (nsend == 0)
			return BAN::Error::from_errno(ECONNRESET);
		total_sent += nsend;
	}

	total_sent = 0;
	while (total_sent < data.size())
	{
		ssize_t nsend = send(fd, data.data() + total_sent, data.size() - total_sent, 0);
		if (nsend == -1)
			return BAN::Error::from_errno(errno);
		if (nsend == 0)
			return BAN::Error::from_errno(ECONNRESET);
		total_sent += nsend;
	}

	return {};
}

BAN::ErrorOr<unsigned> HTTPServer::handle_request(int fd, BAN::Vector<uint8_t>& data)
{
	auto request_or_error = get_http_request(data);
	if (request_or_error.is_error())
		return request_or_error.release_error();
	auto request = request_or_error.release_value();

	dprintln("{} {} {}", request.method, request.path, request.version);

	// remove query string
	if (auto idx = request.path.find('?'); idx.has_value())
		request.path = request.path.substring(0, idx.value());

	// illegal path
	if (request.path.empty() || request.path.front() != '/')
		return 400;

	BAN::StringView path_suffix;
	if (request.path.back() == '/')
		path_suffix = "index.html"_sv;
	else
	{
		auto file = request.path.substring(request.path.rfind('/').value());
		if (!file.contains('.'))
			path_suffix = ".html"_sv;
	}

	auto target_path = TRY(BAN::String::formatted("{}{}{}", m_web_root, request.path, path_suffix));
	auto extension = target_path.sv().substring(target_path.sv().rfind('.').value());

	dprintln("looking for '{}'", target_path);

	char canonical_buffer[PATH_MAX];
	if (realpath(target_path.data(), canonical_buffer) == NULL)
	{
		switch (errno)
		{
			case EACCES:
				return 403;
			case ENAMETOOLONG:
			case ENOENT:
			case ENOTDIR:
				return 404;
			case ELOOP:
				return 500;
			default:
				return BAN::Error::from_errno(errno);
		}
	}
	dprintln("validating '{}'", canonical_buffer);
	if (strncmp(canonical_buffer, m_web_root.data(), m_web_root.size()))
		return BAN::Error::from_errno(403);

	int file_fd = open(canonical_buffer, O_RDONLY);
	if (file_fd == -1)
		return (errno == EACCES) ? 403 : 404;
	BAN::ScopeGuard _([file_fd] { close(file_fd); });

	struct stat file_st;
	if (fstat(file_fd, &file_st) == -1)
		return 500;

	BAN::Vector<uint8_t> file_data;
	if (file_data.resize(file_st.st_size).is_error())
		return 500;

	if (read(file_fd, file_data.data(), file_data.size()) == -1)
		return 500;

	TRY(send_http_response(fd, 200, BAN::ConstByteSpan(file_data.span()), extension_to_mime(extension)));

	return 200;
}

bool HTTPServer::handle_all_requests(int fd, BAN::Vector<uint8_t>& data)
{
	while (true)
	{
		auto result = handle_request(fd, data);
		if (result.is_error() && result.error().get_error_code() == ENODATA)
			return true;
		if (result.is_error())
			return false;
		if (result.value() == 200)
			continue;
		if (send_http_response(fd, result.value(), {}, {}).is_error())
			return false;
	}
}

static BAN::StringView status_to_brief(unsigned status)
{
	static BAN::HashMap<unsigned, BAN::StringView> status_to_brief;
	if (status_to_brief.empty())
	{
		MUST(status_to_brief.emplace(100, "Continue"_sv));
		MUST(status_to_brief.emplace(101, "Switching Protocols"_sv));
		MUST(status_to_brief.emplace(102, "Processing"_sv));
		MUST(status_to_brief.emplace(103, "Early Hints"_sv));

		MUST(status_to_brief.emplace(200, "OK"_sv));
		MUST(status_to_brief.emplace(201, "Created"_sv));
		MUST(status_to_brief.emplace(202, "Accepted"_sv));
		MUST(status_to_brief.emplace(203, "Non-Authoritative Information"_sv));
		MUST(status_to_brief.emplace(204, "No Content"_sv));
		MUST(status_to_brief.emplace(205, "Reset Content"_sv));
		MUST(status_to_brief.emplace(206, "Partial Content"_sv));
		MUST(status_to_brief.emplace(207, "Multi-Status"_sv));
		MUST(status_to_brief.emplace(208, "Already Reported"_sv));
		MUST(status_to_brief.emplace(226, "IM Used"_sv));

		MUST(status_to_brief.emplace(300, "Multiple Choices"_sv));
		MUST(status_to_brief.emplace(301, "Moved Permanently"_sv));
		MUST(status_to_brief.emplace(302, "Found"_sv));
		MUST(status_to_brief.emplace(303, "See Other"_sv));
		MUST(status_to_brief.emplace(304, "Not Modified"_sv));
		MUST(status_to_brief.emplace(305, "Use Proxy"_sv));
		MUST(status_to_brief.emplace(306, "Switch Proxy"_sv));
		MUST(status_to_brief.emplace(307, "Temporary Redirect"_sv));
		MUST(status_to_brief.emplace(308, "Permanent Redirect"_sv));

		MUST(status_to_brief.emplace(400, "Bad Request"_sv));
		MUST(status_to_brief.emplace(401, "Unauthorized"_sv));
		MUST(status_to_brief.emplace(402, "Payment Required Experimental"_sv));
		MUST(status_to_brief.emplace(403, "Forbidden"_sv));
		MUST(status_to_brief.emplace(404, "Not Found"_sv));
		MUST(status_to_brief.emplace(405, "Method Not Allowed"_sv));
		MUST(status_to_brief.emplace(406, "Not Acceptable"_sv));
		MUST(status_to_brief.emplace(407, "Proxy Authentication Required"_sv));
		MUST(status_to_brief.emplace(408, "Request Timeout"_sv));
		MUST(status_to_brief.emplace(409, "Conflict"_sv));
		MUST(status_to_brief.emplace(410, "Gone"_sv));
		MUST(status_to_brief.emplace(411, "Length Required"_sv));
		MUST(status_to_brief.emplace(412, "Precondition Failed"_sv));
		MUST(status_to_brief.emplace(413, "Payload Too Large"_sv));
		MUST(status_to_brief.emplace(414, "URI Too Long"_sv));
		MUST(status_to_brief.emplace(415, "Unsupported Media Type"_sv));
		MUST(status_to_brief.emplace(416, "Range Not Satisfiable"_sv));
		MUST(status_to_brief.emplace(417, "Expectation Failed"_sv));
		MUST(status_to_brief.emplace(418, "I'm a teapot"_sv));
		MUST(status_to_brief.emplace(421, "Misdirected Request"_sv));
		MUST(status_to_brief.emplace(422, "Unprocessable Content (WebDAV)"_sv));
		MUST(status_to_brief.emplace(423, "Locked (WebDAV)"_sv));
		MUST(status_to_brief.emplace(424, "Failed Dependency (WebDAV)"_sv));
		MUST(status_to_brief.emplace(425, "Too Early Experimental"_sv));
		MUST(status_to_brief.emplace(426, "Upgrade Required"_sv));
		MUST(status_to_brief.emplace(428, "Precondition Required"_sv));
		MUST(status_to_brief.emplace(429, "Too Many Requests"_sv));
		MUST(status_to_brief.emplace(431, "Request Header Fields Too Large"_sv));
		MUST(status_to_brief.emplace(451, "Unavailable For Legal Reasons"_sv));

		MUST(status_to_brief.emplace(500, "Internal Server Error"_sv));
		MUST(status_to_brief.emplace(501, "Not Implemented"_sv));
		MUST(status_to_brief.emplace(502, "Bad Gateway"_sv));
		MUST(status_to_brief.emplace(503, "Service Unavailable"_sv));
		MUST(status_to_brief.emplace(504, "Gateway Timeout"_sv));
		MUST(status_to_brief.emplace(505, "HTTP Version Not Supported"_sv));
		MUST(status_to_brief.emplace(506, "Variant Also Negotiates"_sv));
		MUST(status_to_brief.emplace(507, "Insufficient Storage (WebDAV)"_sv));
		MUST(status_to_brief.emplace(508, "Loop Detected (WebDAV)"_sv));
		MUST(status_to_brief.emplace(510, "Not Extended"_sv));
		MUST(status_to_brief.emplace(511, "Network Authentication Required"_sv));
	}

	auto it = status_to_brief.find(status);
	if (it == status_to_brief.end())
		return "unknown"_sv;
	return it->value;
}

static BAN::StringView extension_to_mime(BAN::StringView extension)
{
	static BAN::HashMap<BAN::StringView, BAN::StringView> extension_to_mime;
	if (extension_to_mime.empty())
	{
		MUST(extension_to_mime.emplace(".aac"_sv, "audio/aac"_sv));
		MUST(extension_to_mime.emplace(".abw"_sv, "application/x-abiword"_sv));
		MUST(extension_to_mime.emplace(".apng"_sv, "image/apng"_sv));
		MUST(extension_to_mime.emplace(".arc"_sv, "application/x-freearc"_sv));
		MUST(extension_to_mime.emplace(".avif"_sv, "image/avif"_sv));
		MUST(extension_to_mime.emplace(".avi"_sv, "video/x-msvideo"_sv));
		MUST(extension_to_mime.emplace(".azw"_sv, "application/vnd.amazon.ebook"_sv));
		MUST(extension_to_mime.emplace(".bin"_sv, "application/octet-stream"_sv));
		MUST(extension_to_mime.emplace(".bmp"_sv, "image/bmp"_sv));
		MUST(extension_to_mime.emplace(".bz"_sv, "application/x-bzip"_sv));
		MUST(extension_to_mime.emplace(".bz2"_sv, "application/x-bzip2"_sv));
		MUST(extension_to_mime.emplace(".cda"_sv, "application/x-cdf"_sv));
		MUST(extension_to_mime.emplace(".csh"_sv, "application/x-csh"_sv));
		MUST(extension_to_mime.emplace(".css"_sv, "text/css"_sv));
		MUST(extension_to_mime.emplace(".csv"_sv, "text/csv"_sv));
		MUST(extension_to_mime.emplace(".doc"_sv, "application/msword"_sv));
		MUST(extension_to_mime.emplace(".docx"_sv, "application/vnd.openxmlformats-officedocument.wordprocessingml.document"_sv));
		MUST(extension_to_mime.emplace(".eot"_sv, "application/vnd.ms-fontobject"_sv));
		MUST(extension_to_mime.emplace(".epub"_sv, "application/epub+zip"_sv));
		MUST(extension_to_mime.emplace(".gz"_sv, "application/gzip"_sv));
		MUST(extension_to_mime.emplace(".gif"_sv, "image/gif"_sv));
		MUST(extension_to_mime.emplace(".htm"_sv, "text/html"_sv));
		MUST(extension_to_mime.emplace(".html"_sv, "text/html"_sv));
		MUST(extension_to_mime.emplace(".ico"_sv, "image/vnd.microsoft.icon"_sv));
		MUST(extension_to_mime.emplace(".ics"_sv, "text/calendar"_sv));
		MUST(extension_to_mime.emplace(".jar"_sv, "application/java-archive"_sv));
		MUST(extension_to_mime.emplace(".jpeg"_sv, "image/jpeg"_sv));
		MUST(extension_to_mime.emplace(".jpg"_sv, "image/jpeg"_sv));
		MUST(extension_to_mime.emplace(".js"_sv, "text/javascript"_sv));
		MUST(extension_to_mime.emplace(".json"_sv, "application/json"_sv));
		MUST(extension_to_mime.emplace(".jsonld"_sv, "application/ld+json"_sv));
		MUST(extension_to_mime.emplace(".mid"_sv, "audio/midi, audio/x-midi"_sv));
		MUST(extension_to_mime.emplace(".midi"_sv, "audio/midi, audio/x-midi"_sv));
		MUST(extension_to_mime.emplace(".mjs"_sv, "text/javascript"_sv));
		MUST(extension_to_mime.emplace(".mp3"_sv, "audio/mpeg"_sv));
		MUST(extension_to_mime.emplace(".mp4"_sv, "video/mp4"_sv));
		MUST(extension_to_mime.emplace(".mpeg"_sv, "video/mpeg"_sv));
		MUST(extension_to_mime.emplace(".mpkg"_sv, "application/vnd.apple.installer+xml"_sv));
		MUST(extension_to_mime.emplace(".odp"_sv, "application/vnd.oasis.opendocument.presentation"_sv));
		MUST(extension_to_mime.emplace(".ods"_sv, "application/vnd.oasis.opendocument.spreadsheet"_sv));
		MUST(extension_to_mime.emplace(".odt"_sv, "application/vnd.oasis.opendocument.text"_sv));
		MUST(extension_to_mime.emplace(".oga"_sv, "audio/ogg"_sv));
		MUST(extension_to_mime.emplace(".ogv"_sv, "video/ogg"_sv));
		MUST(extension_to_mime.emplace(".ogx"_sv, "application/ogg"_sv));
		MUST(extension_to_mime.emplace(".opus"_sv, "audio/ogg"_sv));
		MUST(extension_to_mime.emplace(".otf"_sv, "font/otf"_sv));
		MUST(extension_to_mime.emplace(".png"_sv, "image/png"_sv));
		MUST(extension_to_mime.emplace(".pdf"_sv, "application/pdf"_sv));
		MUST(extension_to_mime.emplace(".php"_sv, "application/x-httpd-php"_sv));
		MUST(extension_to_mime.emplace(".ppt"_sv, "application/vnd.ms-powerpoint"_sv));
		MUST(extension_to_mime.emplace(".pptx"_sv, "application/vnd.openxmlformats-officedocument.presentationml.presentation"_sv));
		MUST(extension_to_mime.emplace(".rar"_sv, "application/vnd.rar"_sv));
		MUST(extension_to_mime.emplace(".rtf"_sv, "application/rtf"_sv));
		MUST(extension_to_mime.emplace(".sh"_sv, "application/x-sh"_sv));
		MUST(extension_to_mime.emplace(".svg"_sv, "image/svg+xml"_sv));
		MUST(extension_to_mime.emplace(".tar"_sv, "application/x-tar"_sv));
		MUST(extension_to_mime.emplace(".tif"_sv, "image/tiff"_sv));
		MUST(extension_to_mime.emplace(".tiff"_sv, "image/tiff"_sv));
		MUST(extension_to_mime.emplace(".ts"_sv, "video/mp2t"_sv));
		MUST(extension_to_mime.emplace(".ttf"_sv, "font/ttf"_sv));
		MUST(extension_to_mime.emplace(".txt"_sv, "text/plain"_sv));
		MUST(extension_to_mime.emplace(".vsd"_sv, "application/vnd.visio"_sv));
		MUST(extension_to_mime.emplace(".wav"_sv, "audio/wav"_sv));
		MUST(extension_to_mime.emplace(".weba"_sv, "audio/webm"_sv));
		MUST(extension_to_mime.emplace(".webm"_sv, "video/webm"_sv));
		MUST(extension_to_mime.emplace(".webp"_sv, "image/webp"_sv));
		MUST(extension_to_mime.emplace(".woff"_sv, "font/woff"_sv));
		MUST(extension_to_mime.emplace(".woff2"_sv, "font/woff2"_sv));
		MUST(extension_to_mime.emplace(".xhtml"_sv, "application/xhtml+xml"_sv));
		MUST(extension_to_mime.emplace(".xls"_sv, "application/vnd.ms-excel"_sv));
		MUST(extension_to_mime.emplace(".xlsx"_sv, "application/vnd.openxmlformats-officedocument.spreadsheetml.sheet"_sv));
		MUST(extension_to_mime.emplace(".xml"_sv, "application/xml"_sv));
		MUST(extension_to_mime.emplace(".xul"_sv, "application/vnd.mozilla.xul+xml"_sv));
		MUST(extension_to_mime.emplace(".zip"_sv, "application/zip"_sv));
		MUST(extension_to_mime.emplace(".3gp"_sv, "video/3gpp"_sv));
		MUST(extension_to_mime.emplace(".3g2"_sv, "video/3gpp2"_sv));
		MUST(extension_to_mime.emplace(".7z"_sv, "application/x-7z-compressed"_sv));
	}

	auto it = extension_to_mime.find(extension);
	if (it == extension_to_mime.end())
		return "application/octet-stream"_sv;
	return it->value;
}
