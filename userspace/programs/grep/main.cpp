#include <err.h>
#include <getopt.h>
#include <regex.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct config_t
{
	bool extended     { false };
	bool ignore_case  { false };
	bool only_count   { false };
	bool only_files   { false };
	bool line_numbers { false };
	bool quiet        { false };
	bool no_message   { false };
	bool invert       { false };
	bool multi_file   { false };
};

struct read_line_t
{
	~read_line_t()
	{
		if (!needs_free)
			return;
		free(const_cast<char*>(data));
	}

	constexpr operator bool() const { return data; }

	const char* data;
	const bool needs_free;
};

static read_line_t read_full_line(const char* path, FILE* fp)
{
	static char inline_buffer[16 * 1024];
	if (fgets(inline_buffer, sizeof(inline_buffer), fp) == nullptr)
	{
		if (ferror(fp))
			warn("fgets `%s'", path);
		return { nullptr, false };
	}

	size_t line_len = strlen(inline_buffer);
	size_t line_cap = 2 * sizeof(inline_buffer);

	if (const bool has_newline = inline_buffer[line_len - 1] == '\n'; has_newline || feof(fp))
	{
		if (has_newline)
			inline_buffer[line_len - 1] = '\0';
		return { inline_buffer, false };
	}

	char* line = static_cast<char*>(malloc(line_cap));
	if (line == nullptr)
	{
		warn("malloc `%s'", path);
		return { nullptr, false };
	}

	memcpy(line, inline_buffer, line_len);

	for (;;)
	{
		if (fgets(line + line_len, line_cap - line_len, fp) == nullptr)
		{
			warn("fgets `%s'", path);
			free(line);
			return { nullptr, false };
		}

		line_len += strlen(line + line_len);
		line_cap *= 2;

		if (const bool has_newline = line[line_len - 1] == '\n'; has_newline || feof(fp))
		{
			if (has_newline)
				line[line_len - 1] = '\0';
			return { line, true };
		}

		void* new_line = realloc(line, line_cap);
		if (new_line == nullptr)
		{
			warn("realloc `%s'", path);
			free(line);
			return { nullptr, false };
		}

		line = static_cast<char*>(new_line);
	}
}

static bool do_pattern_matching(const char* path, regex_t regex, config_t config)
{
	FILE* fp;
	if (strcmp(path, "-") == 0)
	{
		path = "(standard input)";
		fp = stdin;
	}
	else if ((fp = fopen(path, "r")) == NULL)
	{
		if (!config.quiet && !config.no_message)
			warn("could not open `%s'", path);
		return false;
	}

	size_t match_count { 0 };
	size_t line_number { 0 };

	while (const auto line = read_full_line(path, fp))
	{
		line_number++;

		if ((regexec(&regex, line.data, 0, nullptr, 0) == 0) == config.invert)
			continue;

		if (config.only_files)
		{
			printf("%s\n", path);
			break;
		}

		if (config.only_count)
			match_count++;
		else
		{
			if (config.multi_file)
				printf("%s:", path);
			if (config.line_numbers)
				printf("%zu:", line_number);
			printf("%s\n", line.data);
		}
	}

	if (match_count > 0)
	{
		if (config.multi_file)
			printf("%s:", path);
		printf("%zu\n", match_count);
	}

	if (fp != stdin)
		fclose(fp);

	return true;
}

int main(int argc, char* argv[])
{
	setprogname(argv[0]);

	config_t config;

	for (;;)
	{
		static option long_options[] {
			{ "extended-regexp",    no_argument, nullptr, 'E' },
			{ "ignore-case",        no_argument, nullptr, 'i' },
			{ "invert-match",       no_argument, nullptr, 'v' },
			{ "count",              no_argument, nullptr, 'c' },
			{ "files-with-matches", no_argument, nullptr, 'l' },
			{ "quiet",              no_argument, nullptr, 'q' },
			{ "no-messages",        no_argument, nullptr, 's' },
			{ "line-number",        no_argument, nullptr, 'n' },
			{ "help",               no_argument, nullptr,  0  },
			{}
		};

		int ch = getopt_long(argc, argv, "Eivclqsn", long_options, nullptr);
		if (ch == -1)
			break;

		switch (ch)
		{
			case 'E':
				config.extended = true;
				break;
			case 'i':
				config.ignore_case = true;
				break;
			case 'v':
				config.invert = true;
				break;
			case 'c':
				config.only_count = true;
				break;
			case 'l':
				config.only_files = true;
				break;
			case 'q':
				config.quiet = true;
				break;
			case 's':
				config.no_message = true;
				break;
			case 'n':
				config.line_numbers = true;
				break;
			case 0:
				fprintf(stderr, "usage: %s [OPTION]... PATTERN [FILE]...\n", argv[0]);
				fprintf(stderr, "  match PATTERN in each FILE\n");
				fprintf(stderr, "OPTIONS:\n");
				fprintf(stderr, "  -E, --extended-regexp      use extender regex for pattern\n");
				fprintf(stderr, "  -i, --ignore-case          do case insensitive matching\n");
				fprintf(stderr, "  -v, --invert-match         print lines not matching the PATTERN\n");
				fprintf(stderr, "  -c, --count                print only number of matched lines\n");
				fprintf(stderr, "  -l, --files-with-matches   print each file that had a line matching PATTERN\n");
				fprintf(stderr, "  -q, --quiet                do not print anything to stdout, only set return code\n");
				fprintf(stderr, "  -s, --no-messages          do not print errors about nonexistent or inaccessible files\n");
				fprintf(stderr, "  -n, --line-number          print line numbers along side the matches\n");
				fprintf(stderr, "      --help                 show this message and exit\n");
				return 0;
			case ':' : case '?':
				fprintf(stderr, "see '%s --help' for usage\n", argv[0]);
				return 1;
		}
	}

	if (optind >= argc)
	{
		fprintf(stderr, "%s: missing operand\n", argv[0]);
		fprintf(stderr, "see '%s --help' for usage\n", argv[0]);
		return 1;
	}

	const char* pattern = argv[optind++];

	regex_t regex;
	if (const int error = regcomp(&regex, pattern, (config.extended ? REG_EXTENDED : 0) | (config.ignore_case ? REG_ICASE : 0)))
	{
		char buffer[128];
		regerror(error, &regex, buffer, sizeof(buffer));
		fprintf(stderr, "%s: could not compile pattern: %s\n", argv[0], buffer);
		return 1;
	}

	config.multi_file = (optind + 1 < argc);

	int ret = 0;
	if (optind >= argc)
	{
		if (!do_pattern_matching("-", regex, config))
			ret = 1;
	}
	else for (int i = optind; i < argc; i++)
	{
		if (!do_pattern_matching(argv[i], regex, config))
			ret = 1;
	}

	return ret;
}
