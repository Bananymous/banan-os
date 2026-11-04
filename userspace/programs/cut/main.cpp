#include <assert.h>
#include <ctype.h>
#include <err.h>
#include <getopt.h>
#include <regex.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define min(a, b) ((a) < (b) ? (a) : (b))

enum class cut_type
{
	none,
	bytes,
	chars,
	fields,
};

struct interval_t
{
	size_t soff;
	size_t eoff;
};

struct config_t
{
	enum cut_type cut_type { cut_type::none };
	interval_t* intervals  { nullptr };
	size_t intervals_len   { 0 };
	bool no_partial        { false };
	bool suppress_no_delim { false };
	char delimeter         { '\t' };
};

static void parse_intervals(const char* string, config_t& out)
{
	size_t intervals_cap { 16 };
	size_t intervals_len { 0 };

	interval_t* intervals = static_cast<interval_t*>(malloc(intervals_cap * sizeof(interval_t)));
	if (intervals == nullptr)
		err(1, "malloc");

	for (;;)
	{
		const auto parse_value = [](const char*& string) -> size_t {
			char* endp;
			const size_t value = strtoull(string, &endp, 10);
			string = endp;
			return value;
		};

		interval_t interval {
			.soff = 1,
			.eoff = SIZE_MAX,
		};

		const bool has_soff = isdigit(*string);
		if (has_soff)
			interval.soff = parse_value(string);

		const bool has_hyphen = (*string == '-');
		if (has_hyphen)
			string++;

		const bool has_eoff = isdigit(*string);
		if (has_eoff)
			interval.eoff = parse_value(string);

		if (has_soff && !has_hyphen)
			interval.eoff = interval.soff;

		if (interval.soff == 0 || interval.eoff == 0)
			errx(1, "intervals start from 1");
		if (!has_soff && !has_eoff)
			errx(1, "invalid interval format");
		if (interval.eoff < interval.soff)
			errx(1, "decreasing range in interval");

		size_t i = 0;
		while (i < intervals_len && intervals[i].eoff < interval.soff - 1)
			i++;

		size_t j = i;
		while (j < intervals_len && intervals[j].soff - 1 <= interval.eoff)
		{
			if (intervals[j].soff < interval.soff)
				interval.soff = intervals[j].soff;

			if (intervals[j].eoff > interval.eoff)
				interval.eoff = intervals[j].eoff;

			j++;
		}

		const size_t new_intervals_len = intervals_len - (j - i) + 1;
		if (new_intervals_len >= intervals_cap)
		{
			intervals_cap *= 2;
			intervals = static_cast<interval_t*>(realloc(intervals, intervals_cap * sizeof(interval_t)));
			if (intervals == nullptr)
				err(1, "realloc");
		}

		memmove(
			intervals + i + 1,
			intervals + j,
			(intervals_len - j) * sizeof(interval_t)
		);
		intervals[i]  = interval;
		intervals_len = new_intervals_len;

		if (*string == '\0')
			break;
		if (*string != ',')
			errx(1, "invalid list format");
		string++;
	}

	out.intervals     = intervals;
	out.intervals_len = intervals_len;
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
	const size_t length;
	const bool needs_free;
};

static read_line_t read_full_line(const char* path, FILE* fp)
{
	static char inline_buffer[16 * 1024];
	if (fgets(inline_buffer, sizeof(inline_buffer), fp) == nullptr)
	{
		if (ferror(fp))
			warn("fgets `%s'", path);
		return { nullptr, 0, false };
	}

	size_t line_len = strlen(inline_buffer);
	size_t line_cap = 2 * sizeof(inline_buffer);

	if (const bool has_newline = inline_buffer[line_len - 1] == '\n'; has_newline || feof(fp))
	{
		if (has_newline)
			inline_buffer[--line_len] = '\0';
		return { inline_buffer, line_len, false };
	}

	char* line = static_cast<char*>(malloc(line_cap));
	if (line == nullptr)
	{
		warn("malloc `%s'", path);
		return { nullptr, 0, false };
	}

	memcpy(line, inline_buffer, line_len);

	for (;;)
	{
		if (fgets(line + line_len, line_cap - line_len, fp) == nullptr)
		{
			warn("fgets `%s'", path);
			free(line);
			return { nullptr, 0, false };
		}

		line_len += strlen(line + line_len);
		line_cap *= 2;

		if (const bool has_newline = line[line_len - 1] == '\n'; has_newline || feof(fp))
		{
			if (has_newline)
				line[--line_len] = '\0';
			return { line, line_len, true };
		}

		void* new_line = realloc(line, line_cap);
		if (new_line == nullptr)
		{
			warn("realloc `%s'", path);
			free(line);
			return { nullptr, 0, false };
		}

		line = static_cast<char*>(new_line);
	}
}

static bool process_file(const char* path, const config_t& config)
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
		switch (config.cut_type)
		{
			case cut_type::none:
				assert(!"unreachable");
			case cut_type::bytes:
				for (size_t i = 0; i < config.intervals_len; i++)
				{
					if (config.intervals[i].soff >= line.length)
						break;

					const char* sstr = line.data +     config.intervals[i].soff               - 1;
					const char* estr = line.data + min(config.intervals[i].eoff, line.length) - 1;
					if (config.no_partial)
					{
						while (sstr > line.data && (static_cast<uint8_t>(*sstr) & 0xC0) == 0x80)
							sstr--;
						while (estr > line.data && (static_cast<uint8_t>(*estr) & 0xC0) == 0x80)
							estr--;
					}

					for (const char* str = sstr; str <= estr; str++)
						putchar(*str);
				}
				putchar('\n');
				break;
			case cut_type::chars:
				for (size_t i = 0, j = 1, k = 0; i < line.length;)
				{
					while (k < config.intervals_len && j > config.intervals[k].eoff)
						k++;
					if (k >= config.intervals_len)
						break;

					int byte_len = 1;
					while (i + byte_len < line.length && (static_cast<uint8_t>(line.data[i + byte_len]) & 0xC0) == 0x80)
						byte_len++;

					if (j <= config.intervals[k].eoff)
						printf("%.*s", byte_len, line.data + i);

					i += byte_len;
					j++;
				}
				putchar('\n');
				break;
			case cut_type::fields:
				bool had_delimiter { false };
				bool has_printed   { false };
				for (size_t i = 0, j = 1, k = 0; i < line.length;)
				{
					while (k < config.intervals_len && j > config.intervals[k].eoff)
						k++;
					if (k >= config.intervals_len)
						break;

					const int length = ({
						const char* delim = strchrnul(line.data + i, config.delimeter);
						if (*delim != '\0')
							had_delimiter = true;
						delim - (line.data + i);
					});

					if (config.intervals[k].soff <= j && j <= config.intervals[k].eoff)
					{
						if (has_printed)
							putchar(config.delimeter);
						printf("%.*s", length, line.data + i);
						has_printed = true;
					}

					i += length + 1;
					j++;
				}
				if (had_delimiter)
					putchar('\n');
				else if (!config.suppress_no_delim)
					printf("%s\n", line.data);
				break;
		}
	}

	if (fp != stdin)
		fclose(fp);

	return true;
}

int main(int argc, char** argv)
{
	setprogname(argv[0]);

	config_t config;

	for (;;)
	{
		static option long_options[] {
			{ "bytes",          required_argument, nullptr, 'b' },
			{ "characters",     required_argument, nullptr, 'c' },
			{ "fields",         required_argument, nullptr, 'f' },
			{ "delimeter",      required_argument, nullptr, 'd' },
			{ "no-partial",     no_argument,       nullptr, 'n' },
			{ "only-delimited", no_argument,       nullptr, 's' },
			{ "help",           no_argument,       nullptr,  0  },
			{}
		};

		int ch = getopt_long(argc, argv, "b:c:f:d:ns", long_options, nullptr);
		if (ch == -1)
			break;

		switch (ch)
		{
			case 'b':
			case 'c':
			case 'f':
				if (config.cut_type != cut_type::none)
				{
					fprintf(stderr, "%s: you must specify only one of -b -c -f\n", argv[0]);
					return 1;
				}
				config.cut_type =
					(ch == 'b') ? cut_type::bytes :
					(ch == 'c') ? cut_type::chars :
					              cut_type::fields;
				parse_intervals(optarg, config);
				break;
			case 'd':
				if (optarg[0] == '\0' || optarg[1] != '\0')
				{
					fprintf(stderr, "%s: delimiter must be a single character\n", argv[0]);
					return 1;
				}
				config.delimeter = optarg[0];
				break;
			case 'n':
				config.no_partial = true;
				break;
			case 's':
				config.suppress_no_delim = true;
				break;
			case 0:
				fprintf(stderr, "usage: %s OPTIONS... LIST [FILE]...\n", argv[0]);
				fprintf(stderr, "  stream editor\n");
				fprintf(stderr, "OPTIONS:\n");
				fprintf(stderr, "  -b, --bytes            cut based on a list of bytes\n");
				fprintf(stderr, "  -c, --characters       cut based on a list of characters\n");
				fprintf(stderr, "  -f, --fields           cut based on a list of fields delimited by delimiter\n");
				fprintf(stderr, "  -d, --delimiter        set the field delimiter, default is <tab>\n");
				fprintf(stderr, "  -n, --no-partial       when -b is specified, don't print out partial characters\n");
				fprintf(stderr, "  -s, --only-delimited   when -f is specified, suppress lines not containing the delimiter\n");
				fprintf(stderr, "      --help             show this message and exit\n");
				return 0;
			case ':' : case '?':
				fprintf(stderr, "see '%s --help' for usage\n", argv[0]);
				return 1;
		}
	}

	if (config.cut_type == cut_type::none)
	{
		fprintf(stderr, "%s: you must specify one of -b -c -f\n", argv[0]);
		return 1;
	}

	int ret = 0;
	if (optind >= argc)
	{
		if (!process_file("-", config))
			ret = 1;
	}
	else for (int i = optind; i < argc; i++)
	{
		if (!process_file(argv[i], config))
			ret = 1;
	}

	return ret;
}
