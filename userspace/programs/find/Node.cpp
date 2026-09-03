#include "Node.h"

#include <fnmatch.h>
#include <grp.h>
#include <pwd.h>
#include <stdarg.h>
#include <sys/stat.h>

bool g_mount_specified { false };
bool g_xdev_specified  { false };
bool g_prune_specified { false };
bool g_depth_specified { false };

static bool s_needs_implicit_print { false };

extern const char* g_argv0;

Node::Node(Type type, decltype(Node::as) as)
	: type(type), as(as)
{
	switch (type)
	{
		case Type::NOD_MOUNT:
			g_mount_specified = true;
			break;
		case Type::NOD_XDEV:
			g_xdev_specified = true;
			break;
		case Type::NOD_PRUNE:
			g_prune_specified = true;
			break;
		case Type::NOD_DEPTH:
			g_depth_specified = true;
			break;
		case Type::NOD_EXEC:
		case Type::NOD_OK:
		case Type::NOD_PRINT:
		case Type::NOD_PRINT0:
			s_needs_implicit_print = false;
			break;
		default:
			break;
	}
}

Node::Node(Type type)
	: Node(type, {})
{ }

static bool token_match(BAN::Span<const Token>& tokens, Token::Type type)
{
	if (tokens[0].type != type)
		return false;
	tokens = tokens.slice(1);
	return true;
}

[[noreturn]] static void print_error_and_exit(const char* format, ...)
{
	va_list args;
	va_start(args, format);
	fprintf(stderr, "%s: ", g_argv0);
	vfprintf(stderr, format, args);
	va_end(args);
	exit(1);
}

static Node* compile_expr(BAN::Span<const Token>& tokens);

static Node* compile_primary(BAN::Span<const Token>& tokens)
{
	constexpr Node::Type type_map[] {
		[0]=(Node::Type)0, [1]=(Node::Type)0,
		[2]=(Node::Type)0, [3]=(Node::Type)0,
		[4]=(Node::Type)0, [5]=(Node::Type)0,
		[6]=(Node::Type)0, [7]=(Node::Type)0,
		[Token::TOK_NAME]    = Node::Type::NOD_NAME,
		[Token::TOK_INAME]   = Node::Type::NOD_INAME,
		[Token::TOK_PATH]    = Node::Type::NOD_PATH,
		[Token::TOK_NOUSER]  = Node::Type::NOD_NOUSER,
		[Token::TOK_NOGROUP] = Node::Type::NOD_NOGROUP,
		[Token::TOK_MOUNT]   = Node::Type::NOD_MOUNT,
		[Token::TOK_XDEV]    = Node::Type::NOD_XDEV,
		[Token::TOK_PRUNE]   = Node::Type::NOD_PRUNE,
		[Token::TOK_PERM]    = Node::Type::NOD_PERM,
		[Token::TOK_TYPE]    = Node::Type::NOD_TYPE,
		[Token::TOK_LINKS]   = Node::Type::NOD_LINKS,
		[Token::TOK_USER]    = Node::Type::NOD_USER,
		[Token::TOK_GROUP]   = Node::Type::NOD_GROUP,
		[Token::TOK_SIZE]    = Node::Type::NOD_SIZE,
		[Token::TOK_ATIME]   = Node::Type::NOD_ATIME,
		[Token::TOK_CTIME]   = Node::Type::NOD_CTIME,
		[Token::TOK_MTIME]   = Node::Type::NOD_MTIME,
		[Token::TOK_EXEC]    = Node::Type::NOD_EXEC,
		[Token::TOK_OK]      = Node::Type::NOD_OK,
		[Token::TOK_PRINT]   = Node::Type::NOD_PRINT,
		[Token::TOK_PRINT0]  = Node::Type::NOD_PRINT0,
		[Token::TOK_NEWER]   = Node::Type::NOD_NEWER,
		[Token::TOK_DEPTH]   = Node::Type::NOD_DEPTH,
	};

	const auto parse_perm = [](const char* string) {
		const bool hyphen = (*string == '-');

		char* endp;
		const mode_t value = strtoul(string + hyphen, &endp, 8);
		if (*endp != '\0')
			print_error_and_exit("invalid perm '%s'\n", string);

		return hyphen ? value | 017777 : value &  07777;
	};

	const auto parse_type = [](const char* string) {
		if (string[1] != '\0');
		else switch (*string)
		{
			case 'b': return S_IFBLK;
			case 'c': return S_IFCHR;
			case 'd': return S_IFDIR;
			case 'l': return S_IFLNK;
			case 'p': return S_IFIFO;
			case 'f': return S_IFREG;
			case 's': return S_IFSOCK;
		}
		print_error_and_exit("invalid type '%s'\n", string);
	};

	const auto parse_n = [](const char* string) -> decltype(Node::as.n) {
		const bool plus  = (*string == '+');
		const bool minus = (*string == '-');

		char* endp;
		const uint64_t value = strtoull(string + (plus || minus), &endp, 10);
		if (*endp != '\0')
			print_error_and_exit("invalid n '%s'\n", string);

		return {
			.value = value,
			.comp = plus ? Node::Comp::Gt : minus ? Node::Comp::Lt : Node::Comp::Eq,
		};
	};

	const auto parse_uid = [](const char* string) -> uid_t {
		if (passwd* passwd = getpwnam(string))
			return passwd->pw_uid;

		char* endp;
		const uid_t value = strtol(string, &endp, 10);
		if (*endp != '\0')
			print_error_and_exit("invalid user '%s'\n", string);

		return value;
	};

	const auto parse_gid = [](const char* string) -> gid_t {
		if (group* group = getgrnam(string))
			return group->gr_gid;

		char* endp;
		const uid_t value = strtol(string, &endp, 10);
		if (*endp != '\0')
			print_error_and_exit("invalid group '%s'\n", string);

		return value;
	};

	const auto parse_newer = [](const char* string) -> timespec {
		// FIXME: this should stat/lstat based on -H/-L/-P
		struct stat st;
		if (stat(string, &st) == -1)
			print_error_and_exit("'%s': %s\n", string, strerror(errno));
		return st.st_mtim;
	};

	const auto error_if_no_arg = [&] {
		if (tokens[1].type == Token::TOK_END)
			print_error_and_exit("missing argument to '%s'\n", tokens[0].string);
	};

	Node* node {};
	if (Token::TOK_NAME <= tokens[0].type && tokens[0].type <= Token::TOK_DEPTH)
		node = new Node { type_map[tokens[0].type] };

	switch (tokens[0].type)
	{
		case Token::TOK_LPAREN:
			tokens = tokens.slice(1);
			node = compile_expr(tokens);
			if (!token_match(tokens, Token::TOK_RPAREN))
				print_error_and_exit("unexpected token '%s'\n", tokens[0].string);
			return node;
		case Token::TOK_NAME:
		case Token::TOK_INAME:
		case Token::TOK_PATH:
			error_if_no_arg();
			node->as.pattern = tokens[1].string;
			tokens = tokens.slice(2);
			return node;
		case Token::TOK_NOUSER:
		case Token::TOK_NOGROUP:
		case Token::TOK_MOUNT:
		case Token::TOK_XDEV:
		case Token::TOK_PRUNE:
			tokens = tokens.slice(1);
			return node;
		case Token::TOK_PERM:
			error_if_no_arg();
			node->as.mode = parse_perm(tokens[1].string);
			tokens = tokens.slice(2);
			return node;
		case Token::TOK_TYPE:
			error_if_no_arg();
			node->as.mode = parse_type(tokens[1].string);
			tokens = tokens.slice(2);
			return node;
		case Token::TOK_LINKS:
		case Token::TOK_ATIME:
		case Token::TOK_CTIME:
		case Token::TOK_MTIME:
			error_if_no_arg();
			node->as.n = parse_n(tokens[1].string);
			tokens = tokens.slice(2);
			return node;
		case Token::TOK_SIZE:
			error_if_no_arg();
			node->as.n = parse_n(tokens[1].string);
			// FIXME: support `-size <n>c`
			node->as.n.value *= 512;
			tokens = tokens.slice(2);
			return node;
		case Token::TOK_USER:
			error_if_no_arg();
			node->as.uid = parse_uid(tokens[1].string);
			tokens = tokens.slice(2);
			return node;
		case Token::TOK_GROUP:
			error_if_no_arg();
			node->as.uid = parse_gid(tokens[1].string);
			tokens = tokens.slice(2);
			return node;
		case Token::TOK_EXEC:
		case Token::TOK_OK:
			print_error_and_exit("unsupported token '%s'\n", tokens[0].string);
		case Token::TOK_PRINT:
		case Token::TOK_PRINT0:
			tokens = tokens.slice(1);
			return node;
		case Token::TOK_NEWER:
			error_if_no_arg();
			node->as.mtime = parse_newer(tokens[1].string);
			tokens = tokens.slice(2);
			return node;
		case Token::TOK_DEPTH:
			tokens = tokens.slice(1);
			return node;
		default:
			print_error_and_exit("unexpected token '%s'\n", tokens[0].string);
	}
}

static Node* compile_not(BAN::Span<const Token>& tokens)
{
	if (!token_match(tokens, Token::TOK_NOT))
		return compile_primary(tokens);

	return new Node {
		Node::Type::NOD_NOT, {
			.child = compile_not(tokens),
		},
	};
}

static Node* compile_and(BAN::Span<const Token>& tokens)
{
	constexpr auto is_implicit_and = [](Token::Type type) {
		switch (type)
		{
			case Token::TOK_AND:
			case Token::TOK_OR:
			case Token::TOK_RPAREN:
			case Token::TOK_END:
				return false;
			default:
				return true;
		}
	};

	Node* node = compile_not(tokens);

	while (token_match(tokens, Token::TOK_AND) || is_implicit_and(tokens[0].type))
	{
		node = new Node {
			Node::Type::NOD_AND, {
				.bin_op = {
					.lhs = node,
					.rhs = compile_not(tokens),
				}
			},
		};
	}

	return node;
}

static Node* compile_or(BAN::Span<const Token>& tokens)
{
	Node* node = compile_and(tokens);

	while (token_match(tokens, Token::TOK_OR))
	{
		node = new Node {
			Node::Type::NOD_OR, {
				.bin_op = {
					.lhs = node,
					.rhs = compile_and(tokens),
				},
			},
		};
	}

	return node;
}

static Node* compile_expr(BAN::Span<const Token>& tokens)
{
	if (tokens[0].type == Token::TOK_END)
		return nullptr;

	return compile_or(tokens);
}

Node* compile_tokens(BAN::Span<const Token> tokens)
{
	s_needs_implicit_print = true;

	Node* node = compile_expr(tokens);

	if (tokens[0].type != Token::TOK_END)
		print_error_and_exit("unexpected token '%s'\n", tokens[0].string);

	if (!s_needs_implicit_print)
		return node;

	if (node == nullptr)
		return new Node { Node::Type::NOD_PRINT };

	return new Node {
		Node::Type::NOD_AND, {
			.bin_op = {
				.lhs = node,
				.rhs = new Node { Node::Type::NOD_PRINT },
			},
		},
	};
}

bool evaluate_node(Node* node, const char* path, struct stat& st)
{
	constexpr auto compare_n = [](uint64_t value, const decltype(Node::as.n)& n) {
		if (n.comp == Node::Comp::Gt)
			return value > n.value;
		if (n.comp == Node::Comp::Lt)
			return value < n.value;
		return value == n.value;
	};

	const auto get_basename = [](const char* path) {
		if (const char* slash = strrchr(path, '/'))
			return slash + 1;
		return path;
	};

	switch (node->type)
	{
		case Node::Type::NOD_AND:
			return evaluate_node(node->as.bin_op.lhs, path, st)
				&& evaluate_node(node->as.bin_op.rhs, path, st);
		case Node::Type::NOD_OR:
			return evaluate_node(node->as.bin_op.lhs, path, st)
				|| evaluate_node(node->as.bin_op.rhs, path, st);
		case Node::Type::NOD_NOT:
			return !evaluate_node(node->as.child, path, st);
		case Node::Type::NOD_NAME:
			return !fnmatch(node->as.pattern, get_basename(path), 0);
		case Node::Type::NOD_INAME:
			return !fnmatch(node->as.pattern, get_basename(path), FNM_CASEFOLD);
		case Node::Type::NOD_PATH:
			return !fnmatch(node->as.pattern, path, 0);
		case Node::Type::NOD_NOUSER:
			return getpwuid(st.st_uid) == nullptr;
		case Node::Type::NOD_NOGROUP:
			return getgrgid(st.st_gid) == nullptr;
		case Node::Type::NOD_TYPE:
			return node->as.mode == (st.st_mode & S_IFMT);
		case Node::Type::NOD_USER:
			return node->as.uid == st.st_uid;
		case Node::Type::NOD_GROUP:
			return node->as.gid == st.st_gid;
		case Node::Type::NOD_PERM:
			if (const mode_t mode = node->as.mode & 07777; mode < node->as.mode)
				return (mode & st.st_mode) == mode;
			return node->as.mode == (st.st_mode & 07777);
		case Node::Type::NOD_LINKS:
			return compare_n(st.st_nlink, node->as.n);
		case Node::Type::NOD_SIZE:
			return compare_n(st.st_size,  node->as.n);
		case Node::Type::NOD_ATIME:
			return compare_n((time(nullptr) - st.st_atime) / 86400, node->as.n);
		case Node::Type::NOD_CTIME:
			return compare_n((time(nullptr) - st.st_ctime) / 86400, node->as.n);
		case Node::Type::NOD_MTIME:
			return compare_n((time(nullptr) - st.st_mtime) / 86400, node->as.n);
		case Node::Type::NOD_NEWER:
			if (st.st_mtim.tv_sec > node->as.mtime.tv_sec)
				return true;
			if (st.st_mtim.tv_sec < node->as.mtime.tv_sec)
				return false;
			return st.st_mtim.tv_nsec > node->as.mtime.tv_nsec;
		case Node::Type::NOD_EXEC:
		case Node::Type::NOD_OK:
			ASSERT_NOT_REACHED();
		case Node::Type::NOD_PRINT:
			fputs(path, stdout);
			fputc('\n', stdout);
			return true;
		case Node::Type::NOD_PRINT0:
			fputs(path, stdout);
			fputc('\0', stdout);
			return true;
		case Node::Type::NOD_MOUNT:
		case Node::Type::NOD_XDEV:
		case Node::Type::NOD_PRUNE:
		case Node::Type::NOD_DEPTH:
			return true;
	}

	ASSERT_NOT_REACHED();
}
