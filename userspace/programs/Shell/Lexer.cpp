#include "Lexer.h"

BAN::ErrorOr<BAN::Vector<Token>> tokenize_string(BAN::StringView string)
{
	{
		size_t i = 0;
		while (i < string.size() && isspace(string[i]))
			i++;
		if (i >= string.size() || string[i] == '#')
			return BAN::Vector<Token>();
	}

	constexpr auto char_to_token_type =
		[](char c) -> BAN::Optional<Token::Type>
		{
			switch (c)
			{
				case '&':  return Token::Type::Ampersand;
				case '\\': return Token::Type::Backslash;
				case '}':  return Token::Type::CloseCurly;
				case ')':  return Token::Type::CloseParen;
				case '$':  return Token::Type::Dollar;
				case '"':  return Token::Type::DoubleQuote;
				case '{':  return Token::Type::OpenCurly;
				case '(':  return Token::Type::OpenParen;
				case '|':  return Token::Type::Pipe;
				case ';':  return Token::Type::Semicolon;
				case '\'': return Token::Type::SingleQuote;
			}
			return {};
		};

	BAN::Vector<Token> result;

	BAN::String current_string;

	const auto append_current_if_exists =
		[&]() -> BAN::ErrorOr<void>
		{
			if (current_string.empty())
				return {};
			TRY(result.emplace_back(Token::Type::String, BAN::move(current_string)));
			current_string = BAN::String();
			return {};
		};

	while (!string.empty())
	{
		if (isspace(string.front()))
		{
			TRY(append_current_if_exists());

			size_t whitespace_len = 1;
			while (whitespace_len < string.size() && isspace(string[whitespace_len]))
				whitespace_len++;

			BAN::String whitespace_str;
			TRY(whitespace_str.append(string.substring(0, whitespace_len)));
			TRY(result.emplace_back(Token::Type::Whitespace, BAN::move(whitespace_str)));
			string = string.substring(whitespace_len);
			continue;
		}

		if (auto token_type = char_to_token_type(string.front()); token_type.has_value())
		{
			TRY(append_current_if_exists());
			TRY(result.emplace_back(token_type.value()));

			string = string.substring(1);
			continue;
		}

		TRY(current_string.push_back(string.front()));
		string = string.substring(1);
	}

	TRY(append_current_if_exists());
	return result;
}
