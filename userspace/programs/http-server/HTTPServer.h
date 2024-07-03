#pragma once

#include <BAN/ByteSpan.h>
#include <BAN/HashMap.h>
#include <BAN/IPv4.h>
#include <BAN/String.h>
#include <BAN/StringView.h>
#include <BAN/Vector.h>

struct HTTPHeader
{
	BAN::StringView name;
	BAN::StringView value;
};

struct HTTPRequest
{
	BAN::StringView method;
	BAN::StringView path;
	BAN::StringView version;

	BAN::Vector<HTTPHeader> headers;
	BAN::ConstByteSpan body;
};

class HTTPServer
{
public:
	HTTPServer();
	~HTTPServer();

	BAN::ErrorOr<void> initialize(BAN::StringView root, BAN::IPv4Address ip, int port);
	void start();

	BAN::StringView web_root() const { return m_web_root.sv(); }

private:
	BAN::ErrorOr<HTTPRequest> get_http_request(BAN::Vector<uint8_t>& data);
	BAN::ErrorOr<void> send_http_response(int fd, unsigned status, BAN::ConstByteSpan, BAN::StringView mime);
	BAN::ErrorOr<unsigned> handle_request(int fd, BAN::Vector<uint8_t>& data);
	// Returns false if the connection should be closed
	bool handle_all_requests(int fd, BAN::Vector<uint8_t>& data);

private:
	BAN::String m_web_root;

	int m_listen_socket { -1 };
	BAN::HashMap<int, BAN::Vector<uint8_t>> m_client_data;
};
