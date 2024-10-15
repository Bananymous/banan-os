#include "Alias.h"
#include "Execute.h"
#include "Lexer.h"
#include "TokenParser.h"

#include <BAN/HashSet.h>

#include <stdio.h>

static constexpr bool can_parse_argument_from_token_type(Token::Type token_type)
{
	switch (token_type)
	{
		case Token::Type::Whitespace:
			ASSERT_NOT_REACHED();
		case Token::Type::EOF_:
		case Token::Type::Ampersand:
		case Token::Type::CloseCurly:
		case Token::Type::CloseParen:
		case Token::Type::OpenCurly:
		case Token::Type::OpenParen:
		case Token::Type::Pipe:
		case Token::Type::Semicolon:
			return false;
		case Token::Type::Backslash:
		case Token::Type::Dollar:
		case Token::Type::DoubleQuote:
		case Token::Type::SingleQuote:
		case Token::Type::String:
			return true;
	}
	ASSERT_NOT_REACHED();
}

static constexpr char token_type_to_single_character(Token::Type type)
{
	switch (type)
	{
		case Token::Type::Ampersand:   return '&';
		case Token::Type::Backslash:   return '\\';
		case Token::Type::CloseCurly:  return '}';
		case Token::Type::CloseParen:  return ')';
		case Token::Type::Dollar:      return '$';
		case Token::Type::DoubleQuote: return '"';
		case Token::Type::OpenCurly:   return '{';
		case Token::Type::OpenParen:   return '(';
		case Token::Type::Pipe:        return '|';
		case Token::Type::Semicolon:   return ';';
		case Token::Type::SingleQuote: return '\'';

		case Token::Type::String:      ASSERT_NOT_REACHED();
		case Token::Type::Whitespace:  ASSERT_NOT_REACHED();
		case Token::Type::EOF_:         ASSERT_NOT_REACHED();
	}
	ASSERT_NOT_REACHED();
};

static constexpr BAN::Error unexpected_token_error(Token::Type type)
{
	switch (type)
	{
		case Token::Type::EOF_:
			return BAN::Error::from_literal("unexpected EOF");
		case Token::Type::Ampersand:
			return BAN::Error::from_literal("unexpected token &");
		case Token::Type::Backslash:
			return BAN::Error::from_literal("unexpected token \\");
		case Token::Type::CloseCurly:
			return BAN::Error::from_literal("unexpected token }");
		case Token::Type::CloseParen:
			return BAN::Error::from_literal("unexpected token )");
		case Token::Type::Dollar:
			return BAN::Error::from_literal("unexpected token $");
		case Token::Type::DoubleQuote:
			return BAN::Error::from_literal("unexpected token \"");
		case Token::Type::OpenCurly:
			return BAN::Error::from_literal("unexpected token {");
		case Token::Type::Pipe:
			return BAN::Error::from_literal("unexpected token |");
		case Token::Type::OpenParen:
			return BAN::Error::from_literal("unexpected token (");
		case Token::Type::Semicolon:
			return BAN::Error::from_literal("unexpected token ;");
		case Token::Type::SingleQuote:
			return BAN::Error::from_literal("unexpected token '");
		case Token::Type::String:
			return BAN::Error::from_literal("unexpected token <string>");
		case Token::Type::Whitespace:
			return BAN::Error::from_literal("unexpected token <whitespace>");
	}
	ASSERT_NOT_REACHED();
}

const Token& TokenParser::peek_token() const
{
	if (m_token_stream.empty())
		return m_eof_token;

	ASSERT(!m_token_stream.front().empty());
	return m_token_stream.front().back();
}

Token TokenParser::read_token()
{
	if (m_token_stream.empty())
		return Token(Token::Type::EOF_);

	ASSERT(!m_token_stream.front().empty());

	auto token = BAN::move(m_token_stream.front().back());
	m_token_stream.front().pop_back();
	if (m_token_stream.front().empty())
		m_token_stream.pop();

	return token;
}

void TokenParser::consume_token()
{
	ASSERT(!m_token_stream.empty());
	ASSERT(!m_token_stream.front().empty());

	m_token_stream.front().pop_back();
	if (m_token_stream.front().empty())
		m_token_stream.pop();
}

BAN::ErrorOr<void> TokenParser::unget_token(Token&& token)
{
	if (m_token_stream.empty())
		TRY(m_token_stream.emplace());
	TRY(m_token_stream.front().push_back(BAN::move(token)));
	return {};
}

BAN::ErrorOr<void> TokenParser::feed_tokens(BAN::Vector<Token>&& tokens)
{
	if (tokens.empty())
		return {};
	for (size_t i = 0; i < tokens.size() / 2; i++)
		BAN::swap(tokens[i], tokens[tokens.size() - i - 1]);
	TRY(m_token_stream.push(BAN::move(tokens)));
	return {};
}

BAN::ErrorOr<void> TokenParser::ask_input_tokens(BAN::StringView prompt, bool add_newline)
{
	if (!m_input_function)
		return unexpected_token_error(Token::Type::EOF_);

	auto opt_input = m_input_function(prompt);
	if (!opt_input.has_value())
		return unexpected_token_error(Token::Type::EOF_);

	auto tokenized = TRY(tokenize_string(opt_input.release_value()));
	TRY(feed_tokens(BAN::move(tokenized)));

	if (add_newline)
	{
		auto newline_token = Token(Token::Type::String);
		TRY(newline_token.string().push_back('\n'));
		TRY(unget_token(BAN::move(newline_token)));
	}

	return {};
}

BAN::ErrorOr<CommandArgument::ArgumentPart> TokenParser::parse_backslash(bool is_quoted)
{
	ASSERT(read_token().type() == Token::Type::Backslash);

	auto token = read_token();

	CommandArgument::FixedString fixed_string;
	switch (token.type())
	{
		case Token::Type::EOF_:
			TRY(ask_input_tokens("> ", false));
			TRY(unget_token(Token(Token::Type::Backslash)));
			return parse_backslash(is_quoted);
		case Token::Type::Ampersand:
		case Token::Type::Backslash:
		case Token::Type::CloseCurly:
		case Token::Type::CloseParen:
		case Token::Type::Dollar:
		case Token::Type::DoubleQuote:
		case Token::Type::OpenCurly:
		case Token::Type::OpenParen:
		case Token::Type::Pipe:
		case Token::Type::Semicolon:
		case Token::Type::SingleQuote:
			TRY(fixed_string.value.push_back(token_type_to_single_character(token.type())));
			break;
		case Token::Type::Whitespace:
		case Token::Type::String:
		{
			ASSERT(!token.string().empty());
			if (is_quoted)
				TRY(fixed_string.value.push_back('\\'));
			TRY(fixed_string.value.push_back(token.string().front()));
			if (token.string().size() > 1)
			{
				token.string().remove(0);
				TRY(unget_token(BAN::move(token)));
			}
			break;
		}
	}

	return CommandArgument::ArgumentPart(BAN::move(fixed_string));
}

BAN::ErrorOr<CommandArgument::ArgumentPart> TokenParser::parse_dollar()
{
	ASSERT(read_token().type() == Token::Type::Dollar);

	const auto parse_dollar_string =
		[](BAN::String& string) -> BAN::ErrorOr<CommandArgument::ArgumentPart>
		{
			if (string.empty())
				return CommandArgument::ArgumentPart(CommandArgument::EnvironmentVariable());
			if (isdigit(string.front()))
			{
				size_t number_len = 1;
				while (number_len < string.size() && isdigit(string[number_len]))
					number_len++;

				CommandArgument::BuiltinVariable builtin;
				TRY(builtin.value.append(string.sv().substring(0, number_len)));
				for (size_t i = 0; i < number_len; i++)
					string.remove(0);

				return CommandArgument::ArgumentPart(BAN::move(builtin));
			}
			switch (string.front())
			{
				case '$':
				case '_':
				case '@':
				case '*':
				case '#':
				case '-':
				case '?':
				case '!':
				{
					CommandArgument::BuiltinVariable builtin;
					TRY(builtin.value.push_back(string.front()));
					string.remove(0);
					return CommandArgument::ArgumentPart(BAN::move(builtin));
				}
			}
			if (isalpha(string.front()))
			{
				size_t env_len = 1;
				while (env_len < string.size() && (isalnum(string[env_len]) || string[env_len] == '_'))
					env_len++;

				CommandArgument::EnvironmentVariable environment;
				TRY(environment.value.append(string.sv().substring(0, env_len)));
				for (size_t i = 0; i < env_len; i++)
					string.remove(0);

				return CommandArgument::ArgumentPart(BAN::move(environment));
			}

			CommandArgument::FixedString fixed_string;
			TRY(fixed_string.value.push_back('$'));
			return CommandArgument::ArgumentPart(BAN::move(fixed_string));
		};

	switch (peek_token().type())
	{
		case Token::Type::EOF_:
		case Token::Type::Ampersand:
		case Token::Type::Backslash:
		case Token::Type::CloseCurly:
		case Token::Type::CloseParen:
		case Token::Type::DoubleQuote:
		case Token::Type::Pipe:
		case Token::Type::Semicolon:
		case Token::Type::SingleQuote:
		case Token::Type::Whitespace:
		{
			CommandArgument::FixedString fixed_string;
			TRY(fixed_string.value.push_back('$'));
			return CommandArgument::ArgumentPart(BAN::move(fixed_string));
		}
		case Token::Type::Dollar:
		{
			consume_token();

			CommandArgument::BuiltinVariable builtin_variable;
			TRY(builtin_variable.value.push_back('$'));
			return CommandArgument::ArgumentPart(BAN::move(builtin_variable));
		}
		case Token::Type::OpenCurly:
		{
			consume_token();

			BAN::String input;
			for (auto token = read_token(); token.type() != Token::Type::CloseCurly; token = read_token())
			{
				if (token.type() == Token::Type::EOF_)
					return BAN::Error::from_literal("missing closing curly brace");

				if (token.type() == Token::Type::String)
					TRY(input.append(token.string()));
				else if (token.type() == Token::Type::Dollar)
					TRY(input.push_back('$'));
				else
					return BAN::Error::from_literal("expected closing curly brace");
			}

			auto result = TRY(parse_dollar_string(input));
			if (!input.empty())
				return BAN::Error::from_literal("bad substitution");
			return result;
		}
		case Token::Type::OpenParen:
		{
			consume_token();

			auto command_tree = TRY(parse_command_tree());
			if (auto token = read_token(); token.type() != Token::Type::CloseParen)
				return BAN::Error::from_literal("expected closing parenthesis");
			return CommandArgument::ArgumentPart(BAN::move(command_tree));
		}
		case Token::Type::String:
		{
			auto token = read_token();

			auto string = BAN::move(token.string());
			auto result = TRY(parse_dollar_string(string));
			if (!string.empty())
			{
				auto remaining = Token(Token::Type::String);
				remaining.string() = BAN::move(string);
				TRY(unget_token(BAN::move(remaining)));
			}
			return result;
		}
	}

	ASSERT_NOT_REACHED();
}

BAN::ErrorOr<CommandArgument::ArgumentPart> TokenParser::parse_single_quote()
{
	ASSERT(read_token().type() == Token::Type::SingleQuote);

	CommandArgument::FixedString fixed_string;
	for (auto token = read_token();; token = read_token())
	{
		switch (token.type())
		{
			case Token::Type::EOF_:
				TRY(ask_input_tokens("quote> ", true));
				break;
			case Token::Type::Ampersand:
			case Token::Type::Backslash:
			case Token::Type::CloseCurly:
			case Token::Type::CloseParen:
			case Token::Type::Dollar:
			case Token::Type::DoubleQuote:
			case Token::Type::OpenCurly:
			case Token::Type::OpenParen:
			case Token::Type::Pipe:
			case Token::Type::Semicolon:
				TRY(fixed_string.value.push_back(token_type_to_single_character(token.type())));
				break;
			case Token::Type::String:
			case Token::Type::Whitespace:
				TRY(fixed_string.value.append(token.string()));
				break;
			case Token::Type::SingleQuote:
				return CommandArgument::ArgumentPart(BAN::move(fixed_string));
		}
	}
}

BAN::ErrorOr<CommandArgument> TokenParser::parse_argument()
{
	using FixedString = CommandArgument::FixedString;

	const auto token_type = peek_token().type();
	if (!can_parse_argument_from_token_type(token_type))
		return unexpected_token_error(token_type);

	CommandArgument result;

	bool is_in_double_quotes = false;
	for (auto token_type = peek_token().type(); token_type != Token::Type::EOF_ || is_in_double_quotes; token_type = peek_token().type())
	{
		CommandArgument::ArgumentPart new_part;
		switch (token_type)
		{
			case Token::Type::EOF_:
				ASSERT(is_in_double_quotes);
				TRY(ask_input_tokens("dquote> ", true));
				new_part = FixedString(); // do continue
				break;
			case Token::Type::Ampersand:
			case Token::Type::CloseCurly:
			case Token::Type::CloseParen:
			case Token::Type::OpenCurly:
			case Token::Type::OpenParen:
			case Token::Type::Pipe:
			case Token::Type::Semicolon:
				if (is_in_double_quotes)
				{
					new_part = FixedString();
					TRY(new_part.get<FixedString>().value.push_back(token_type_to_single_character(token_type)));
					consume_token();
				}
				break;
			case Token::Type::Whitespace:
				if (is_in_double_quotes)
				{
					new_part = FixedString();
					TRY(new_part.get<FixedString>().value.append(peek_token().string()));
					consume_token();
				}
				break;
			case Token::Type::Backslash:
				new_part = TRY(parse_backslash(is_in_double_quotes));
				break;
			case Token::Type::DoubleQuote:
				is_in_double_quotes = !is_in_double_quotes;
				new_part = FixedString(); // do continue
				consume_token();
				break;
			case Token::Type::Dollar:
				new_part = TRY(parse_dollar());
				break;
			case Token::Type::SingleQuote:
				new_part = TRY(parse_single_quote());
				break;
			case Token::Type::String:
				new_part = CommandArgument::ArgumentPart(FixedString {});
				TRY(new_part.get<FixedString>().value.append(peek_token().string()));
				consume_token();
				break;
		}

		if (!new_part.has_value())
			break;

		if (new_part.has<FixedString>())
		{
			auto& fixed_string = new_part.get<FixedString>();
			// discard empty fixed strings
			if (fixed_string.value.empty())
				continue;
			// combine consecutive fixed strings
			if (!result.parts.empty() && result.parts.back().has<FixedString>())
			{
				TRY(result.parts.back().get<FixedString>().value.append(fixed_string.value));
				continue;
			}
		}

		TRY(result.parts.push_back(BAN::move(new_part)));
	}

	return result;
}

BAN::ErrorOr<SingleCommand> TokenParser::parse_single_command()
{
	SingleCommand result;

	while (peek_token().type() == Token::Type::Whitespace)
		consume_token();

	while (peek_token().type() == Token::Type::String)
	{
		BAN::String env_name;

		const auto& string = peek_token().string();
		if (!isalpha(string.front()))
			break;

		const auto env_len = string.sv().find([](char ch) { return !(isalnum(ch) || ch == '_'); });
		if (!env_len.has_value() || string[*env_len] != '=')
			break;
		TRY(env_name.append(string.sv().substring(0, *env_len)));

		auto full_value = TRY(parse_argument());
		ASSERT(!full_value.parts.empty());
		ASSERT(full_value.parts.front().has<CommandArgument::FixedString>());

		auto& first_arg = full_value.parts.front().get<CommandArgument::FixedString>();
		ASSERT(first_arg.value.sv().starts_with(env_name));
		ASSERT(first_arg.value[*env_len] == '=');
		for (size_t i = 0; i < *env_len + 1; i++)
			first_arg.value.remove(0);
		if (first_arg.value.empty())
			full_value.parts.remove(0);

		SingleCommand::EnvironmentVariable environment_variable;
		environment_variable.name = BAN::move(env_name);
		environment_variable.value = BAN::move(full_value);
		TRY(result.environment.emplace_back(BAN::move(environment_variable)));

		while (peek_token().type() == Token::Type::Whitespace)
			consume_token();
	}

	BAN::HashSet<BAN::String> used_aliases;
	while (peek_token().type() == Token::Type::String)
	{
		auto token = read_token();

		bool can_be_alias = false;
		switch (peek_token().type())
		{
			case Token::Type::EOF_:
			case Token::Type::Ampersand:
			case Token::Type::CloseParen:
			case Token::Type::Pipe:
			case Token::Type::Semicolon:
			case Token::Type::Whitespace:
				can_be_alias = true;
				break;
			case Token::Type::Backslash:
			case Token::Type::CloseCurly:
			case Token::Type::Dollar:
			case Token::Type::DoubleQuote:
			case Token::Type::OpenCurly:
			case Token::Type::OpenParen:
			case Token::Type::SingleQuote:
			case Token::Type::String:
				can_be_alias = false;
				break;
		}
		if (!can_be_alias)
		{
			TRY(unget_token(BAN::move(token)));
			break;
		}

		if (used_aliases.contains(token.string()))
		{
			TRY(unget_token(BAN::move(token)));
			break;
		}

		auto opt_alias = Alias::get().get_alias(token.string().sv());
		if (!opt_alias.has_value())
		{
			TRY(unget_token(BAN::move(token)));
			break;
		}

		auto tokenized_alias = TRY(tokenize_string(opt_alias.value()));
		for (size_t i = tokenized_alias.size(); i > 0; i--)
			TRY(unget_token(BAN::move(tokenized_alias[i - 1])));
		TRY(used_aliases.insert(TRY(BAN::String::formatted("{}", token.string()))));

		while (peek_token().type() == Token::Type::Whitespace)
			consume_token();
	}

	while (peek_token().type() != Token::Type::EOF_)
	{
		while (peek_token().type() == Token::Type::Whitespace)
			consume_token();

		auto argument = TRY(parse_argument());
		TRY(result.arguments.push_back(BAN::move(argument)));

		while (peek_token().type() == Token::Type::Whitespace)
			consume_token();
		if (!can_parse_argument_from_token_type(peek_token().type()))
			break;
	}

	return result;
}

BAN::ErrorOr<PipedCommand> TokenParser::parse_piped_command()
{
	PipedCommand result;
	result.background = false;

	while (peek_token().type() != Token::Type::EOF_)
	{
		auto single_command = TRY(parse_single_command());
		TRY(result.commands.push_back(BAN::move(single_command)));

		const auto token_type = peek_token().type();
		if (token_type != Token::Type::Pipe && token_type != Token::Type::Ampersand)
			break;

		auto temp_token = read_token();
		if (peek_token().type() == temp_token.type())
		{
			TRY(unget_token(BAN::move(temp_token)));
			break;
		}

		if (temp_token.type() == Token::Type::Ampersand)
		{
			result.background = true;
			break;
		}
	}

	return result;
}

BAN::ErrorOr<CommandTree> TokenParser::parse_command_tree()
{
	CommandTree result;

	auto next_condition = ConditionalCommand::Condition::Always;
	while (peek_token().type() != Token::Type::EOF_)
	{
		ConditionalCommand conditional_command;
		conditional_command.command = TRY(parse_piped_command());
		conditional_command.condition = next_condition;
		TRY(result.commands.push_back(BAN::move(conditional_command)));

		while (peek_token().type() == Token::Type::Whitespace)
			consume_token();
		if (peek_token().type() == Token::Type::EOF_)
			break;

		bool should_break = false;
		const auto token_type = peek_token().type();
		switch (token_type)
		{
			case Token::Type::Semicolon:
				consume_token();
				next_condition = ConditionalCommand::Condition::Always;
				break;
			case Token::Type::Ampersand:
			case Token::Type::Pipe:
				consume_token();
				if (read_token().type() != token_type)
					return BAN::Error::from_literal("expected double '&' or '|'");
				next_condition = (token_type == Token::Type::Ampersand)
					? ConditionalCommand::Condition::OnSuccess
					: ConditionalCommand::Condition::OnFailure;
				break;
			default:
				should_break = true;
				break;
		}

		if (should_break)
			break;
	}

	return result;
}

BAN::ErrorOr<void> TokenParser::run(BAN::Vector<Token>&& tokens)
{
	TRY(feed_tokens(BAN::move(tokens)));

	auto command_tree = TRY(parse_command_tree());

	const auto token_type = peek_token().type();
	while (!m_token_stream.empty())
		m_token_stream.pop();

	if (token_type != Token::Type::EOF_)
		return unexpected_token_error(token_type);

	TRY(m_execute.execute_command(command_tree));
	return {};
}

bool TokenParser::main_loop(bool break_on_error)
{
	for (;;)
	{
		auto opt_input = m_input_function({});
		if (!opt_input.has_value())
			break;

		auto tokenized_input = tokenize_string(opt_input.release_value());
		if (tokenized_input.is_error())
		{
			fprintf(stderr, "banan-sh: %s\n", tokenized_input.error().get_message());
			if (break_on_error)
				return false;
			continue;
		}

		if (auto ret = run(tokenized_input.release_value()); ret.is_error())
		{
			fprintf(stderr, "banan-sh: %s\n", ret.error().get_message());
			if (break_on_error)
				return false;
			continue;
		}
	}

	return true;
}
