#include <BAN/String.h>
#include <BAN/Vector.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#define STR_STARTS_WITH(str, arg) (strncmp(str, arg, sizeof(arg) - 1) == 0)
#define STR_EQUAL(str, arg) (strcmp(str, arg) == 0)

bool copy_file(const BAN::String& source, BAN::String destination)
{
	struct stat st;
	if (stat(source.data(), &st) == -1)
	{
		fprintf(stderr, "%s: ", source.data());
		perror("stat");
		return false;
	}
	if (!S_ISREG(st.st_mode))
	{
		fprintf(stderr, "%s: not a directory\n", source.data());
		return false;
	}

	if (stat(destination.data(), &st) != -1 && S_ISDIR(st.st_mode))
	{
		MUST(destination.push_back('/'));
		MUST(destination.append(MUST(source.sv().split('/')).back()));
	}

	int src_fd = open(source.data(), O_RDONLY);
	if (src_fd == -1)
	{
		fprintf(stderr, "%s: ", source.data());
		perror("open");
		return false;
	}

	int dest_fd = open(destination.data(), O_CREAT | O_TRUNC | O_WRONLY, 0644);
	if (dest_fd == -1)
	{
		fprintf(stderr, "%s: ", destination.data());
		perror("open");
		close(src_fd);
		return false;
	}

	bool ret = true;
	char buffer[1024];
	while (ssize_t nread = read(src_fd, buffer, sizeof(buffer)))
	{
		if (nread < 0)
		{
			fprintf(stderr, "%s: ", source.data());
			perror("read");
			ret = false;
			break;
		}

		size_t written = 0;
		while (written < nread)
		{
			ssize_t nwrite = write(dest_fd, buffer, nread - written);
			if (nwrite < 0)
			{
				fprintf(stderr, "%s: ", destination.data());
				perror("write");
				ret = false;
			}
			if (nwrite <= 0)
				break;
			written += nwrite;
		}

		if (written < nread)
			break;
	}

	close(src_fd);
	close(dest_fd);
	return ret;
}

bool copy_file_to_directory(const BAN::String& source, const BAN::String& destination)
{
	auto temp = destination;
	MUST(temp.append(MUST(source.sv().split('/')).back()));
	return copy_file(source, destination);
}

void usage(const char* argv0, int ret)
{
	FILE* out = (ret == 0) ? stdout : stderr;
	fprintf(out, "usage: %s [OPTIONS]... SOURCE... DEST\n", argv0);
	fprintf(out, "Copies files SOURCE... to DEST\n");
	fprintf(out, "OPTIONS:\n");
	fprintf(out, "    -h, --help\n");
	fprintf(out, "        Show this message and exit\n");
	exit(ret);
}

int main(int argc, char** argv)
{
	BAN::Vector<BAN::StringView> src;
	BAN::StringView dest;

	int i = 1;
	for (; i < argc; i++)
	{
		if (STR_EQUAL(argv[i], "-h") || STR_EQUAL(argv[i], "--help"))
		{
			usage(argv[0], 0);
		}
		else if (argv[i][0] == '-')
		{
			fprintf(stderr, "Unknown argument %s\n", argv[i]);
			usage(argv[0], 1);
		}
		else
		{
			break;
		}
	}

	for (; i < argc - 1; i++)
		MUST(src.push_back(argv[i]));
	dest = argv[argc - 1];

	if (src.empty())
	{
		fprintf(stderr, "Missing destination operand\n");
		usage(argv[0], 1);
	}

	int ret = 0;
	for (auto file_path : src)
		if (!copy_file(file_path, dest))
			ret = 1;

	return ret;
}
