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
#include <sys/select.h>
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
	INVALID = 0x0000,
	A		= 0x0001,
	CNAME	= 0x0005,
	AAAA	= 0x001C,
};

struct DNSEntry
{
	DNSEntry(BAN::IPv4Address&& address, time_t valid_until)
		: type(QTYPE::A)
		, valid_until(valid_until)
		, address(BAN::move(address))
	{}

	DNSEntry(BAN::String&& cname, time_t valid_until)
		: type(QTYPE::CNAME)
		, valid_until(valid_until)
		, cname(BAN::move(cname))
	{}

	DNSEntry(DNSEntry&& other)
	{
		*this = BAN::move(other);
	}

	~DNSEntry() { clear(); }

	DNSEntry& operator=(DNSEntry&& other)
	{
		clear();
		valid_until = other.valid_until;
		switch (type = other.type)
		{
			case QTYPE::A:
				new (&address) BAN::IPv4Address(BAN::move(other.address));
				break;
			case QTYPE::CNAME:
				new (&cname) BAN::String(BAN::move(other.cname));
				break;
			case QTYPE::INVALID:
			case QTYPE::AAAA:
				ASSERT_NOT_REACHED();
		}
		other.clear();
		return *this;
	}

	void clear()
	{
		switch (type)
		{
			case QTYPE::A:
				using BAN::IPv4Address;
				address.~IPv4Address();
				break;
			case QTYPE::CNAME:
				using BAN::String;
				cname.~String();
				break;
			case QTYPE::AAAA:
				ASSERT_NOT_REACHED();
			case QTYPE::INVALID:
				break;
		}
		type = QTYPE::INVALID;
	}

	QTYPE type;
	time_t valid_until;
	union {
		BAN::IPv4Address address;
		BAN::String cname;
	};
};

struct DNSResponse
{
	struct NameEntryPair
	{
		BAN::String name;
		DNSEntry entry;
	};

	uint16_t id;
	BAN::Vector<NameEntryPair> entries;
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

BAN::Optional<DNSResponse> read_dns_response(int socket)
{
	static uint8_t buffer[4096];

	ssize_t nrecv = recvfrom(socket, buffer, sizeof(buffer), 0, nullptr, nullptr);
	if (nrecv == -1)
	{
		dprintln("recvfrom: {}", strerror(errno));
		return {};
	}

	DNSPacket& reply = *reinterpret_cast<DNSPacket*>(buffer);

	DNSResponse result;
	result.id = reply.identification;

	if (reply.flags & 0x0F)
	{
		dprintln("DNS error (rcode {})", (unsigned)(reply.flags & 0xF));
		return result;
	}

	size_t idx = reply.data - buffer;
	for (size_t i = 0; i < reply.question_count; i++)
	{
		while (buffer[idx])
			idx += buffer[idx] + 1;
		idx += 5;
	}

	const auto read_name =
		[](size_t idx) -> BAN::String
		{
			BAN::String result;
			while (buffer[idx])
			{
				if ((buffer[idx] & 0xC0) == 0xC0)
				{
					idx = ((buffer[idx] & 0x3F) << 8) | buffer[idx + 1];
					continue;
				}

				MUST(result.append(BAN::StringView(reinterpret_cast<const char*>(&buffer[idx + 1]), buffer[idx])));
				MUST(result.push_back('.'));
				idx += buffer[idx] + 1;
			}

			if (!result.empty())
				result.pop_back();
			return result;
		};

	for (size_t i = 0; i < reply.answer_count; i++)
	{
		auto& answer = *reinterpret_cast<DNSAnswer*>(&buffer[idx]);

		auto name = read_name(answer.__storage - buffer);

		if (answer.type() == QTYPE::A)
		{
			if (answer.data_len() != 4)
			{
				dprintln("Invalid A record size {}", (uint16_t)answer.data_len());
				return result;
			}

			MUST(result.entries.push_back({
				.name = BAN::move(name),
				.entry = {
					BAN::IPv4Address(*reinterpret_cast<uint32_t*>(answer.data)),
					time(nullptr) + answer.ttl(),
				},
			}));
		}
		else if (answer.type() == QTYPE::CNAME)
		{
			auto target = read_name(answer.data - buffer);

			MUST(result.entries.push_back({
				.name = BAN::move(name),
				.entry = {
					BAN::move(target),
					time(nullptr) + answer.ttl()
				},
			}));
		}

		idx += sizeof(DNSAnswer) + answer.data_len();
	}

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

BAN::Optional<BAN::IPv4Address> resolve_from_dns_cache(BAN::HashMap<BAN::String, DNSEntry>& dns_cache, const BAN::String& domain)
{
	for (auto it = dns_cache.find(domain); it != dns_cache.end();)
	{
		if (time(nullptr) > it->value.valid_until)
		{
			dns_cache.remove(it);
			return {};
		}

		switch (it->value.type)
		{
			case QTYPE::A:
				return it->value.address;
			case QTYPE::CNAME:
				it = dns_cache.find(it->value.cname);
				break;
			case QTYPE::AAAA:
			case QTYPE::INVALID:
				ASSERT_NOT_REACHED();
		}
	}

	return {};
}

int main(int, char**)
{
	srand(time(nullptr));

	char hostname[HOST_NAME_MAX];
	if (gethostname(hostname, sizeof(hostname)) == -1)
		hostname[0] = '\0';

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

	struct Client
	{
		Client(int socket)
			: socket(socket)
		{ }
		const int	socket;
		bool		close { false };
		uint16_t	query_id { 0 };
		BAN::String	query;
	};

	BAN::LinkedList<Client> clients;

	for (;;)
	{
		int max_sock = BAN::Math::max(service_socket, dns_socket);

		fd_set fds;
		FD_ZERO(&fds);
		FD_SET(service_socket, &fds);
		FD_SET(dns_socket, &fds);
		for (auto& client : clients)
		{
			FD_SET(client.socket, &fds);
			max_sock = BAN::Math::max(max_sock, client.socket);
		}

		int nselect = select(max_sock + 1, &fds, nullptr, nullptr, nullptr);
		if (nselect == -1)
		{
			perror("select");
			continue;
		}

		if (FD_ISSET(service_socket, &fds))
		{
			int client = accept(service_socket, nullptr, nullptr);
			if (client == -1)
			{
				perror("accept");
				continue;
			}

			MUST(clients.emplace_back(client));
		}

		if (FD_ISSET(dns_socket, &fds))
		{
			auto result = read_dns_response(dns_socket);
			if (!result.has_value())
				continue;

			for (auto&& [name, entry] : result->entries)
				MUST(dns_cache.insert_or_assign(BAN::move(name), BAN::move(entry)));

			for (auto& client : clients)
			{
				if (client.query_id != result->id)
					continue;

				auto resolved = resolve_from_dns_cache(dns_cache, client.query);
				if (!resolved.has_value())
				{
					auto it = dns_cache.find(client.query);
					if (it == dns_cache.end())
					{
						client.close = true;
						break;
					}
					for (;;)
					{
						ASSERT(it->value.type == QTYPE::CNAME);
						auto next = dns_cache.find(it->value.cname);
						if (next == dns_cache.end())
							break;
						it = next;
					}
					send_dns_query(service_socket, it->value.cname, client.query_id);
					break;
				}

				const sockaddr_in addr {
					.sin_family = AF_INET,
					.sin_port = 0,
					.sin_addr = { .s_addr = resolved->raw },
				};

				if (send(client.socket, &addr, sizeof(addr), 0) == -1)
					dprintln("send: {}", strerror(errno));
				client.close = true;
				break;
			}
		}

		for (auto& client : clients)
		{
			if (!FD_ISSET(client.socket, &fds))
				continue;

			if (!client.query.empty())
			{
				static uint8_t buffer[4096];
				ssize_t nrecv = recv(client.socket, buffer, sizeof(buffer), 0);
				if (nrecv < 0)
					dprintln("{}", strerror(errno));
				if (nrecv <= 0)
					client.close = true;
				else
					dprintln("Client already has a query");
				continue;
			}

			auto query = read_service_query(client.socket);
			if (!query.has_value())
			{
				client.close = true;
				continue;
			}

			BAN::Optional<BAN::IPv4Address> result;

			if (*hostname && strcmp(query->data(), hostname) == 0)
				result = BAN::IPv4Address(ntohl(INADDR_LOOPBACK));
			else if (auto resolved = resolve_from_dns_cache(dns_cache, query.value()); resolved.has_value())
				result = resolved.release_value();

			if (result.has_value())
			{
				const sockaddr_in addr {
					.sin_family = AF_INET,
					.sin_port = 0,
					.sin_addr = { .s_addr = result->raw },
				};

				if (send(client.socket, &addr, sizeof(addr), 0) == -1)
					dprintln("send: {}", strerror(errno));
				client.close = true;
				continue;
			}

			client.query = query.release_value();
			client.query_id = rand() % 0xFFFF;
			send_dns_query(dns_socket, client.query, client.query_id);
		}

		for (auto it = clients.begin(); it != clients.end();)
		{
			if (!it->close)
			{
				it++;
				continue;
			}

			close(it->socket);
			it = clients.remove(it);
		}
	}

	return 0;
}
