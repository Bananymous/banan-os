#include "HTTPServer.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>

int usage(const char* argv0, int ret)
{
	FILE* fout = (ret == 0) ? stdout : stderr;
	fprintf(fout, "usage: %s [OPTIONS]...\n", argv0);
	fprintf(fout, "  -h, --help          show this message and exit\n");
	fprintf(fout, "  -r, --root <PATH>   web root directory\n");
	fprintf(fout, "  -b, --bind <IPv4>   local address to bind\n");
	fprintf(fout, "  -p, --port <PORT>   local port to bind\n");
	return ret;
}

int main(int argc, char** argv)
{
	BAN::StringView root = "/var/www"_sv;
	BAN::IPv4Address bind = INADDR_ANY;
	uint16_t port = 80;

	for (int i = 1; i < argc; i++)
	{
		if (strcmp(argv[i], "-r") == 0 || strcmp(argv[i], "--root") == 0)
		{
			if (i + 1 >= argc)
				return usage(argv[0], 1);
			root = argv[i + 1];
			i++;
		}
		else if (strcmp(argv[i], "-b") == 0 || strcmp(argv[i], "--bind") == 0)
		{
			if (i + 1 >= argc)
				return usage(argv[0], 1);
			bind = inet_addr(argv[i + 1]);
			if (bind.raw == (in_addr_t)(-1))
				return usage(argv[0], 1);
			i++;
		}
		else if (strcmp(argv[i], "-p") == 0 || strcmp(argv[i], "--port") == 0)
		{
			if (i + 1 >= argc)
				return usage(argv[0], 1);
			char* end = NULL;
			errno = 0;
			unsigned long value = strtoul(argv[i + 1], &end, 10);
			if (*end || value > 0xFFFF || errno)
				return usage(argv[0], 1);
			port = value;
			i++;
		}
		else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0)
		{
			return usage(argv[0], 0);
		}
		else
		{
			return usage(argv[0], 1);
		}
	}

	HTTPServer server;
	if (auto ret = server.initialize(root, bind, port); ret.is_error())
	{
		fprintf(stderr, "Could not initialize server: %s\n", strerror(ret.error().get_error_code()));
		return 1;
	}

	BAN::Formatter::println(putchar, "Server started on {}:{} at {}", bind, port, server.web_root());
	server.start();
}
