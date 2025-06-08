#include <BAN/Optional.h>
#include <BAN/Span.h>
#include <BAN/StringView.h>

#include <libgen.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

const char* argv0 = nullptr;

[[noreturn]] void exit_on_error(const char* format, ...)
{
	fprintf(stderr, "%s: ", argv0);
	va_list args;
	va_start(args, format);
	vfprintf(stderr, format, args);
	va_end(args);
	exit(2);
	__builtin_unreachable();
}

long long parse_integer(const char* string)
{
	errno = 0;
	char* endptr;
	long long value = strtoll(string, &endptr, 0);
	if (*endptr == '\0' && errno == 0)
		return value;
	exit_on_error("integer expression expected, got %s\n", string);
}

bool check_file_mode(const char* pathname, mode_t mask, mode_t mode)
{
	const auto func = (mode == S_IFLNK) ? lstat : stat;
	struct stat st;
	if (func(pathname, &st) == -1)
		return false;
	return (st.st_mode & mask) == mode;
}

bool check_file_not_empty(const char* pathname)
{
	struct stat st;
	if (stat(pathname, &st) == -1)
		return false;
	return st.st_size > 0;
}

BAN::Optional<bool> evaluate_file_op(BAN::Span<const char*>& args)
{
	struct FileOp
	{
		char name;
		bool (*func)(const char*);
	};

	constexpr FileOp file_ops[] {
		{ 'b', [](auto* s) { return check_file_mode(s, S_IFMT,  S_IFBLK);  } },
		{ 'c', [](auto* s) { return check_file_mode(s, S_IFMT,  S_IFCHR);  } },
		{ 'd', [](auto* s) { return check_file_mode(s, S_IFMT,  S_IFDIR);  } },
		{ 'e', [](auto* s) { return check_file_mode(s, 0,       0      );  } },
		{ 'f', [](auto* s) { return check_file_mode(s, S_IFMT,  S_IFREG);  } },
		{ 'g', [](auto* s) { return check_file_mode(s, S_ISGID, S_ISGID);  } },
		{ 'h', [](auto* s) { return check_file_mode(s, S_IFMT,  S_IFLNK);  } },
		{ 'L', [](auto* s) { return check_file_mode(s, S_IFMT,  S_IFLNK);  } },
		{ 'p', [](auto* s) { return check_file_mode(s, S_IFMT,  S_IFIFO);  } },
		{ 'S', [](auto* s) { return check_file_mode(s, S_IFMT,  S_IFSOCK); } },
		{ 'u', [](auto* s) { return check_file_mode(s, S_ISUID, S_ISUID);  } },
		{ 's', [](auto* s) { return check_file_not_empty(s);               } },
		{ 'r', [](auto* s) { return access(s, R_OK) == 0;                  } },
		{ 'w', [](auto* s) { return access(s, W_OK) == 0;                  } },
		{ 'x', [](auto* s) { return access(s, X_OK) == 0;                  } },
	};

	if (args.size() < 2)
		return {};
	if (args[0][0] != '-' || args[0][1] == '\0' || args[0][2] != '\0')
		return {};

	for (const auto& file_op : file_ops)
	{
		if (args[0][1] != file_op.name)
			continue;
		const char* pathname = args[1];
		args = args.slice(2);
		return file_op.func(pathname);
	}

	return {};
}

BAN::Optional<bool> evaluate_string_op(BAN::Span<const char*>& args)
{
	if (args.size() < 3)
		return {};
	if (args[1] != "="_sv && args[1] != "!="_sv)
		return {};

	const bool result = (args[1] == "="_sv) == (strcmp(args[0], args[2]) == 0);
	args = args.slice(3);
	return result;
}

BAN::Optional<bool> evaluate_numeric_op(BAN::Span<const char*>& args)
{
	if (args.size() < 3)
		return {};

	struct NumericOp
	{
		BAN::StringView name;
		bool (*func)(long long, long long);
	};

	constexpr NumericOp numeric_ops[] {
		{ "-eq", [](auto val1, auto val2) { return val1 == val2; } },
		{ "-ne", [](auto val1, auto val2) { return val1 != val2; } },
		{ "-gt", [](auto val1, auto val2) { return val1 >  val2; } },
		{ "-ge", [](auto val1, auto val2) { return val1 >= val2; } },
		{ "-lt", [](auto val1, auto val2) { return val1 <  val2; } },
		{ "-le", [](auto val1, auto val2) { return val1 <= val2; } },
	};

	for (const auto& numeric_op : numeric_ops)
	{
		if (args[1] != numeric_op.name)
			continue;
		auto val1 = parse_integer(args[0]);
		auto val2 = parse_integer(args[2]);
		args = args.slice(3);
		return numeric_op.func(val1, val2);
	}

	return {};
}

bool evaluate(BAN::Span<const char*>& args);

bool evaluate_expression(BAN::Span<const char*>& args)
{
	if (args.empty())
		return false;

	if (args.size() == 1 || args[1] == "-o"_sv || args[1] == "-a"_sv)
	{
		const bool result = (args[0] != ""_sv);
		args = args.slice(1);
		return result;
	}

	// the string comparison binary primaries '=' and "!=" shall have a higher
	// precedence than any unary primary
	if (auto result = evaluate_string_op(args); result.has_value())
		return false;

	if (args[0] == "!"_sv)
	{
		args = args.slice(1);
		return !evaluate_expression(args);
	}

	if (args[0] == "-z"_sv || args[0] == "-n"_sv)
	{
		const bool want_empty = (args[0] == "-z"_sv);
		const bool is_empty = (args[1] == ""_sv);
		args = args.slice(2);
		return want_empty == is_empty;
	}

	if (args[0] == "-t"_sv)
	{
		auto value = parse_integer(args[1]);
		args = args.slice(2);
		if (value < 0 || value > INT_MAX)
			return false;
		return isatty(value);
	}

	if (auto result = evaluate_file_op(args); result.has_value())
		return result.value();

	if (auto result = evaluate_numeric_op(args); result.has_value())
		return result.value();

	if (args[0] == "("_sv)
	{
		args = args.slice(1);

		const bool value = evaluate(args);
		if (args.empty() || args[0] != ")"_sv)
			exit_on_error("missing ')'\n");

		args = args.slice(1);
		return value;
	}

	const bool result = args[0] != ""_sv;
	args = args.slice(1);
	return result;
}

bool evaluate(BAN::Span<const char*>& args)
{
	bool value = evaluate_expression(args);

	while (!args.empty())
	{
		// NOTE: POSIX says -a has higher precedence than -o, but other
		//       implementations just do it as left associative.
		//       Even linux man page says: 'Binary -a and -o are ambiguous.'

		if (args[0] != "-o"_sv && args[0] != "-a"_sv)
			break;

		const bool op_and = (args[0] == "-a"_sv);

		args = args.slice(1);
		const bool rhs = evaluate_expression(args);
		value = op_and ? (value && rhs) : (value || rhs);
	}

	return value;
}

int main(int argc, const char** argv)
{
	argv0 = argv[0];

	char argv0_copy[PATH_MAX];
	strcpy(argv0_copy, argv0);

	if (strcmp(basename(argv0_copy), "[") == 0)
	{
		if (strcmp(argv[argc - 1], "]") != 0)
			exit_on_error("missing ']'\n");
		argc--;
	}

	auto args = BAN::Span(argv + 1, argc - 1);
	const bool result = evaluate(args);
	if (!args.empty())
		exit_on_error("parse error near '%s'\n", args[0]);
	return result ? 0 : 1;
}
