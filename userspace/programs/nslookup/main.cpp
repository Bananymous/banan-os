#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>

#define MAX(a, b) ((a) < (b) ? (b) : (a))

int main(int argc, char** argv)
{
	if (argc != 2)
	{
		fprintf(stderr, "usage: %s DOMAIN\n", argv[0]);
		return 1;
	}

	const addrinfo hints {
		.ai_flags = 0,
		.ai_family = AF_INET,
		.ai_socktype = SOCK_STREAM,
		.ai_protocol = 0,
		.ai_addrlen = 0,
		.ai_addr = nullptr,
		.ai_canonname = nullptr,
		.ai_next = nullptr,
	};

	addrinfo* result;
	if (int ret = getaddrinfo(argv[1], nullptr, &hints, &result); ret != 0)
	{
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(ret));
		return 1;
	}

	for (addrinfo* ai = result; ai; ai = ai->ai_next)
	{
		if (ai->ai_family != AF_INET)
			continue;

		char buffer[NI_MAXHOST];
		if (inet_ntop(ai->ai_family, &reinterpret_cast<sockaddr_in*>(ai->ai_addr)->sin_addr, buffer, sizeof(buffer)) == nullptr)
			continue;

		printf("%s\n", buffer);
		return 0;
	}

	fprintf(stderr, "no address information available\n");
	return 0;
}
