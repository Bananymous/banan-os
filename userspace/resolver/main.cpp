#include <BAN/ByteSpan.h>
#include <BAN/Endianness.h>
#include <BAN/HashMap.h>
#include <BAN/IPv4.h>
#include <BAN/String.h>
#include <BAN/StringView.h>
#include <BAN/Vector.h>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <unistd.h>

struct DNSPacket
{
	BAN::NetworkEndian<uint16_t> identification			{ 0 };
	BAN::NetworkEndian<uint16_t> flags					{ 0 };
	BAN::NetworkEndian<uint16_t> question_count			{ 0 };
	BAN::NetworkEndian<uint16_t> answer_count			{ 0 };
	BAN::NetworkEndian<uint16_t> authority_RR_count		{ 0 };
	BAN::NetworkEndian<uint16_t> additional_RR_count	{ 0 };
	uint8_t data[];
};
static_assert(sizeof(DNSPacket) == 12);

struct DNSAnswer
{
	uint8_t __storage[12];
	BAN::NetworkEndian<uint16_t>& name()		{ return *reinterpret_cast<BAN::NetworkEndian<uint16_t>*>(__storage + 0x00); };
	BAN::NetworkEndian<uint16_t>& type()		{ return *reinterpret_cast<BAN::NetworkEndian<uint16_t>*>(__storage + 0x02); };
	BAN::NetworkEndian<uint16_t>& class_()		{ return *reinterpret_cast<BAN::NetworkEndian<uint16_t>*>(__storage + 0x04); };
	BAN::NetworkEndian<uint32_t>& ttl()			{ return *reinterpret_cast<BAN::NetworkEndian<uint32_t>*>(__storage + 0x06); };
	BAN::NetworkEndian<uint16_t>& data_len()	{ return *reinterpret_cast<BAN::NetworkEndian<uint16_t>*>(__storage + 0x0A); };
	uint8_t data[];
};
static_assert(sizeof(DNSAnswer) == 12);

bool send_dns_query(int socket, BAN::StringView domain, uint16_t id)
{
	static uint8_t buffer[4096];
	memset(buffer, 0, sizeof(buffer));

	DNSPacket& request = *reinterpret_cast<DNSPacket*>(buffer);
	request.identification	= id;
	request.flags			= 0x0100;
	request.question_count	= 1;

	size_t idx = 0;

	auto labels = MUST(BAN::StringView(domain).split('.'));
	for (auto label : labels)
	{
		ASSERT(label.size() <= 0xFF);
		request.data[idx++] = label.size();
		for (char c : label)
			request.data[idx++] = c;
	}
	request.data[idx++] = 0x00;

	*(uint16_t*)&request.data[idx] = htons(0x01); idx += 2;
	*(uint16_t*)&request.data[idx] = htons(0x01); idx += 2;

	sockaddr_in nameserver;
	nameserver.sin_family = AF_INET;
	nameserver.sin_port = htons(53);
	nameserver.sin_addr.s_addr = inet_addr("8.8.8.8");
	if (sendto(socket, &request, sizeof(DNSPacket) + idx, 0, (sockaddr*)&nameserver, sizeof(nameserver)) == -1)
	{
		perror("sendto");
		return false;
	}

	return true;
}

BAN::Optional<BAN::String> read_dns_response(int socket, uint16_t id)
{
	static uint8_t buffer[4096];

	ssize_t nrecv = recvfrom(socket, buffer, sizeof(buffer), 0, nullptr, nullptr);
	if (nrecv == -1)
	{
		perror("recvfrom");
		return {};
	}

	DNSPacket& reply = *reinterpret_cast<DNSPacket*>(buffer);
	if (reply.identification != id)
	{
		fprintf(stderr, "Reply to invalid packet\n");
		return {};
	}
	if (reply.flags & 0x0F)
	{
		fprintf(stderr, "DNS error (rcode %u)\n", (unsigned)(reply.flags & 0xF));
		return {};
	}

	size_t idx = 0;
	for (size_t i = 0; i < reply.question_count; i++)
	{
		while (reply.data[idx])
			idx += reply.data[idx] + 1;
		idx += 5;
	}

	DNSAnswer& answer = *reinterpret_cast<DNSAnswer*>(&reply.data[idx]);
	if (answer.data_len() != 4)
	{
		fprintf(stderr, "Not IPv4\n");
		return {};
	}

	return inet_ntoa({ .s_addr = *reinterpret_cast<uint32_t*>(answer.data) });
}

int create_service_socket()
{
	int socket = ::socket(AF_UNIX, SOCK_SEQPACKET, 0);
	if (socket == -1)
	{
		perror("socket");
		return -1;
	}

	sockaddr_un addr;
	addr.sun_family = AF_UNIX;
	strcpy(addr.sun_path, "/tmp/resolver.sock");
	if (bind(socket, (sockaddr*)&addr, sizeof(addr)) == -1)
	{
		perror("bind");
		close(socket);
		return -1;
	}

	if (chmod("/tmp/resolver.sock", 0777) == -1)
	{
		perror("chmod");
		close(socket);
		return -1;
	}

	if (listen(socket, 10) == -1)
	{
		perror("listen");
		close(socket);
		return -1;
	}

	return socket;
}

BAN::Optional<BAN::String> read_service_query(int socket)
{
	static char buffer[4096];
	ssize_t nrecv = recv(socket, buffer, sizeof(buffer), 0);
	if (nrecv == -1)
	{
		perror("recv");
		return {};
	}
	buffer[nrecv] = '\0';
	return BAN::String(buffer);
}

int main(int, char**)
{
	srand(time(nullptr));

	int service_socket = create_service_socket();
	if (service_socket == -1)
		return 1;

	int dns_socket = socket(AF_INET, SOCK_DGRAM, 0);
	if (dns_socket == -1)
	{
		perror("socket");
		return 1;
	}

	for (;;)
	{
		int client = accept(service_socket, nullptr, nullptr);
		if (client == -1)
		{
			perror("accept");
			continue;
		}

		auto query = read_service_query(client);
		if (!query.has_value())
			continue;

		uint16_t id = rand() % 0xFFFF;

		if (send_dns_query(dns_socket, *query, id))
		{
			auto response = read_dns_response(dns_socket, id);
			if (response.has_value())
			{
				if (send(client, response->data(), response->size() + 1, 0) == -1)
					perror("send");
				close(client);
				continue;
			}
		}

		char message[] = "unavailable";
		send(client, message, sizeof(message), 0);
		close(client);
	}

	return 0;
}
