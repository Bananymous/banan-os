#include <assert.h>
#include <getopt.h>
#include <stdio.h>
#include <string.h>

char* optarg = nullptr;
int opterr = 1;
int optind = 1;
int optopt = 0;

static int s_idx_in_arg = -1;
static int s_old_optind = 1;

static int getopt_long_impl(int argc, char* const argv[], const char* optstring, const struct option* longopts, int* longindex, bool need_double_hyphen)
{
	if (optind >= argc)
		return -1;

	if (optind == 0)
	{
		s_old_optind = -1;
		s_idx_in_arg = -1;
		optind = 1;
	}

	const char* curr = argv[optind];
	if (curr == nullptr || curr[0] != '-' || curr[1] == '\0')
		return -1;

	if (curr[1] == '-' && curr[2] == '\0')
	{
		optind++;
		return -1;
	}

	if (s_old_optind != optind)
		s_idx_in_arg = -1;
	struct dummy { ~dummy() { s_old_optind = optind; }} _;

	if (s_idx_in_arg == -1 && (!need_double_hyphen || curr[1] == '-'))
	{
		for (size_t i = 0; longopts[i].name; i++)
		{
			const auto& opt = longopts[i];
			const size_t name_len = strlen(opt.name);
			if (strncmp(curr + 2, opt.name, name_len) != 0)
				continue;
			if (curr[2 + name_len] != '=' && curr[2 + name_len] != '\0')
				continue;

			bool has_argument;
			switch (opt.has_arg)
			{
				case no_argument:
					has_argument = false;
					break;
				case required_argument:
					has_argument = true;
					break;
				case optional_argument:
					has_argument = (curr[2 + name_len] == '=')
						|| (optind + 1 < argc && argv[optind + 1][0] != '-');
					break;
				default:
					assert(false);
			}

			if (!has_argument)
			{
				if (curr[2 + name_len] == '=')
				{
					if (opterr && optstring[0] != ':')
						fprintf(stderr, "%s: option takes no argument -- %.*s\n", argv[0], static_cast<int>(name_len), curr + 2);
					optind++;
					return (optstring[0] == ':') ? ':' : '?';
				}
				optarg = nullptr;
				optind++;
			}
			else
			{
				if (curr[2 + name_len] == '=')
				{
					optarg = const_cast<char*>(curr + 2 + name_len + 1);
					optind++;
				}
				else
				{
					if (optind + 1 >= argc)
					{
						if (opterr && optstring[0] != ':')
							fprintf(stderr, "%s: option requires an argument -- %.*s\n", argv[0], static_cast<int>(name_len), curr + 2);
						optind++;
						return (optstring[0] == ':') ? ':' : '?';
					}
					optarg = argv[optind + 1];
					optind += 2;
				}
			}

			if (longindex != nullptr)
				*longindex = i;

			if (opt.flag == nullptr)
				return opt.val;
			*opt.flag = opt.val;
			return 0;
		}

		if (curr[1] == '-')
		{
			if (opterr && optstring[0] != ':')
				fprintf(stderr, "%s: illegal option -- %s\n", argv[0], curr + 2);
			optind++;
			return '?';
		}
	}

	if (s_idx_in_arg == -1)
		s_idx_in_arg = 1;

	for (size_t i = 0; optstring[i]; i++)
	{
		if (optstring[i] == ':')
			continue;
		if (curr[s_idx_in_arg] != optstring[i])
			continue;

		const bool has_argument = (optstring[i + 1] == ':');
		if (!has_argument)
		{
			s_idx_in_arg++;
			if (curr[s_idx_in_arg] == '\0')
			{
				s_idx_in_arg = -1;
				optind++;
			}
		}
		else
		{
			if (curr[s_idx_in_arg + 1] != '\0')
			{
				optarg = const_cast<char*>(curr + s_idx_in_arg + 1);
				optind++;
				s_idx_in_arg = -1;
			}
			else
			{
				if (optind + 1 >= argc)
				{
					if (opterr && optstring[0] != ':')
						fprintf(stderr, "%s: option requires an argument -- %c\n", argv[0], optstring[i]);
					optopt = optstring[i];
					optind++;
					return optstring[0] == ':' ? ':' : '?';
				}
				optarg = const_cast<char*>(argv[optind + 1]);
				optind += 2;
				s_idx_in_arg = -1;
			}
		}

		return optstring[i];
	}

	if (opterr && optstring[0] != ':')
		fprintf(stderr, "%s: illegal option -- %c\n", argv[0], curr[s_idx_in_arg]);
	optopt = curr[s_idx_in_arg];

	s_idx_in_arg++;
	if (curr[s_idx_in_arg] == '\0')
	{
		s_idx_in_arg = -1;
		optind++;
	}

	return '?';
}

int getopt(int argc, char* const argv[], const char* optstring)
{
	struct option option {};
	return getopt_long_impl(argc, argv, optstring, &option, nullptr, true);
}

int getopt_long(int argc, char* argv[], const char* optstring, const struct option* longopts, int* longindex)
{
	return getopt_long_impl(argc, argv, optstring, longopts, longindex, true);
}

int getopt_long_only(int argc, char* argv[], const char* optstring, const struct option* longopts, int* longindex)
{
	return getopt_long_impl(argc, argv, optstring, longopts, longindex, false);
}
