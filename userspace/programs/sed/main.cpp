#include <err.h>
#include <getopt.h>
#include <regex.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct substitution_t
{
	regex_t regex;
	const char* substitution;
	bool global;
};

static substitution_t parse_substitution(const char* string, bool extended)
{
	char* head = strdup(string);
	if (head == nullptr)
		err(1, "strdup");

	if (*head++ != 's')
		errx(1, "only substitutions are supported");

	const char delimeter = *head++;
	if (delimeter == '\0')
		errx(1, "invalid substitution `%s'", string);

	const char* pattern = head;
	while (*head && *head != delimeter)
		head++;
	if (*head == '\0')
		errx(1, "invalid substitution `%s'", string);
	*head++ = '\0';

	const char* substitution = head;
	while (*head && *head != delimeter)
		head++;
	if (*head == '\0')
		errx(1, "invalid substitution `%s'", string);
	*head++ = '\0';

	bool global { false };
	int flags = extended ? REG_EXTENDED : 0;
	while (*head)
	{
		switch (*head)
		{
			case 'i':
				flags |= REG_ICASE;
				break;
			case 'g':
				global = true;
				break;
			default:
				errx(1, "unsupported flag `%c'", *head);
		}
		head++;
	}

	regex_t regex;
	if (int error = regcomp(&regex, pattern, flags))
	{
		char buffer[128];
		regerror(error, &regex, buffer, sizeof(buffer));
		errx(1, "invalid pattern `%s': %s", pattern, buffer);
	}

	return substitution_t {
		.regex = regex,
		.substitution = substitution,
		.global = global,
	};
}

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

static bool process_file(const char* path, const substitution_t& substitution)
{
	FILE* fp;
	if (strcmp(path, "-") == 0)
	{
		path = "(standard input)";
		fp = stdin;
	}
	else if ((fp = fopen(path, "r")) == NULL)
	{
		warn("could not open `%s'", path);
		return false;
	}

	while (const auto line = read_full_line(path, fp))
	{
		regmatch_t match {};

		const char* buffer = line.data;
		while (regexec(&substitution.regex, buffer, 1, &match, match.rm_eo ? REG_NOTBOL : 0) == 0)
		{
			printf("%.*s%s", static_cast<int>(match.rm_so), buffer, substitution.substitution);
			buffer += match.rm_eo;
			if (!substitution.global)
				break;
		}

		printf("%s\n", buffer);
	}

	if (fp != stdin)
		fclose(fp);

	return true;
}

int main(int argc, char** argv)
{
	setprogname(argv[0]);

	bool extended_regex { false };

	for (;;)
	{
		static option long_options[] {
			{ "regexp-extended", no_argument, nullptr, 'E' },
			{ "help",            no_argument, nullptr,  0  },
			{}
		};

		int ch = getopt_long(argc, argv, "E", long_options, nullptr);
		if (ch == -1)
			break;

		switch (ch)
		{
			case 'E':
				extended_regex = true;
				break;
			case 0:
				fprintf(stderr, "usage: %s [OPTION]... EXPRESSION [FILE]...\n", argv[0]);
				fprintf(stderr, "  stream editor\n");
				fprintf(stderr, "OPTIONS:\n");
				fprintf(stderr, "  -E, --regexp-extended   use extended regular expressions\n");
				fprintf(stderr, "      --help              show this message and exit\n");
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

	auto substitution = parse_substitution(argv[optind++], extended_regex);

	int ret = 0;
	if (optind >= argc)
	{
		if (!process_file("-", substitution))
			ret = 1;
	}
	else for (int i = optind; i < argc; i++)
	{
		if (!process_file(argv[i], substitution))
			ret = 1;
	}

	return ret;
}
