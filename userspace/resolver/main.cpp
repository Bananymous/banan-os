#include <BAN/ByteSpan.h>
#include <BAN/Debug.h>
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

enum QTYPE : uint16_t
{
	A		= 0x0001,
	CNAME	= 0x0005,
	AAAA	= 0x001C,
};

struct DNSEntry
{
	time_t				valid_until	{ 0 };
	BAN::IPv4Address	address 	{ 0 };
};

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

	*(uint16_t*)&request.data[idx] = htons(QTYPE::A); idx += 2;
	*(uint16_t*)&request.data[idx] = htons(0x0001); idx += 2;

	sockaddr_in nameserver;
	nameserver.sin_family = AF_INET;
	nameserver.sin_port = htons(53);
	nameserver.sin_addr.s_addr = inet_addr("8.8.8.8");
	if (sendto(socket, &request, sizeof(DNSPacket) + idx, 0, (sockaddr*)&nameserver, sizeof(nameserver)) == -1)
	{
		dprintln("sendto: {}", strerror(errno));
		return false;
	}

	return true;
}

BAN::Optional<DNSEntry> read_dns_response(int socket, uint16_t id)
{
	static uint8_t buffer[4096];

	ssize_t nrecv = recvfrom(socket, buffer, sizeof(buffer), 0, nullptr, nullptr);
	if (nrecv == -1)
	{
		dprintln("recvfrom: {}", strerror(errno));
		return {};
	}

	DNSPacket& reply = *reinterpret_cast<DNSPacket*>(buffer);
	if (reply.identification != id)
	{
		dprintln("Reply to invalid packet");
		return {};
	}
	if (reply.flags & 0x0F)
	{
		dprintln("DNS error (rcode {})", (unsigned)(reply.flags & 0xF));
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
	if (answer.type() != QTYPE::A)
	{
		dprintln("Not A record");
		return {};
	}
	if (answer.data_len() != 4)
	{
		dprintln("corrupted package");
		return {};
	}

	DNSEntry result;
	result.valid_until	= time(nullptr) + answer.ttl();
	result.address		= BAN::IPv4Address(*reinterpret_cast<uint32_t*>(answer.data));

	return result;
}

int create_service_socket()
{
	int socket = ::socket(AF_UNIX, SOCK_SEQPACKET, 0);
	if (socket == -1)
	{
		dprintln("socket: {}", strerror(errno));
		return -1;
	}

	sockaddr_un addr;
	addr.sun_family = AF_UNIX;
	strcpy(addr.sun_path, "/tmp/resolver.sock");
	if (bind(socket, (sockaddr*)&addr, sizeof(addr)) == -1)
	{
		dprintln("bind: {}", strerror(errno));
		close(socket);
		return -1;
	}

	if (chmod("/tmp/resolver.sock", 0777) == -1)
	{
		dprintln("chmod: {}", strerror(errno));
		close(socket);
		return -1;
	}

	if (listen(socket, 10) == -1)
	{
		dprintln("listen: {}", strerror(errno));
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
		dprintln("recv: {}", strerror(errno));
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
		dprintln("socket: {}", strerror(errno));
		return 1;
	}

	BAN::HashMap<BAN::String, DNSEntry> dns_cache;

	for (;;)
	{
		int client = accept(service_socket, nullptr, nullptr);
		if (client == -1)
		{
			dprintln("accept: {}", strerror(errno));
			continue;
		}

		auto query = read_service_query(client);
		if (!query.has_value())
		{
			close(client);
			continue;
		}

		BAN::Optional<DNSEntry> result;

		if (dns_cache.contains(*query))
		{
			auto& cached = dns_cache[*query];
			if (time(nullptr) <= cached.valid_until)
				result = cached;
			else
				dns_cache.remove(*query);
		}

		if (!result.has_value())
		{
			uint16_t id = rand() % 0xFFFF;
			if (send_dns_query(dns_socket, *query, id))
			{
				result = read_dns_response(dns_socket, id);
				if (result.has_value())
					(void)dns_cache.insert(*query, *result);
			}
		}

		if (!result.has_value())
			result = DNSEntry { .valid_until = 0, .address = BAN::IPv4Address(INADDR_ANY) };

		sockaddr_storage storage;
		storage.ss_family = AF_INET;
		memcpy(storage.ss_storage, &result->address.raw, sizeof(result->address.raw));

		if (send(client, &storage, sizeof(storage), 0) == -1)
			dprintln("send: {}", strerror(errno));

		close(client);
	}

	return 0;
}
