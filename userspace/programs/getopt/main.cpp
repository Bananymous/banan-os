#include <BAN/String.h>
#include <BAN/Vector.h>
#include <stdio.h>
#include <unistd.h>

int main(int argc, char** argv)
{
	if (argc < 2)
	{
		fprintf(stderr, "usage: %s OPTSTRING [PARAMETERS]...", argv[0]);
		return 1;
	}

	BAN::Vector<char*> argv_copy(argc - 1);
	argv_copy[0] = argv[0];
	for (int i = 2; i < argc; i++)
		argv_copy[i - 1] = argv[i];

	int opt;
	BAN::String parsed;
	while ((opt = getopt(argc - 1, argv_copy.data(), argv[1])) != -1)
	{
		if (opt == ':' || opt == '?')
			continue;

		MUST(parsed.append(" -"));
		MUST(parsed.push_back(opt));

		if (optarg)
		{
			MUST(parsed.push_back(' '));
			MUST(parsed.append(optarg));
		}

		optarg = nullptr;
	}

	printf("%s --", parsed.data());
	for (; optind < argc - 1; optind++)
		printf(" %s", argv_copy[optind]);
	printf("\n");

	return 0;
}
