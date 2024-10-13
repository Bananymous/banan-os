#pragma once

#include "CommandTypes.h"
#include "Execute.h"
#include "Token.h"

#include <BAN/Function.h>
#include <BAN/NoCopyMove.h>
#include <BAN/Optional.h>
#include <BAN/Queue.h>
#include <BAN/Vector.h>

class TokenParser
{
	BAN_NON_COPYABLE(TokenParser);
	BAN_NON_MOVABLE(TokenParser);
public:
	using InputFunction = BAN::Function<BAN::Optional<BAN::String>(BAN::Optional<BAN::StringView>)>;

public:
	TokenParser(const InputFunction& input_function)
		: m_input_function(input_function)
	{ }

	Execute& execute() { return m_execute; }
	const Execute& execute() const { return m_execute; }

	[[nodiscard]] bool main_loop(bool break_on_error);

private:
	const Token& peek_token() const;
	Token read_token();
	void consume_token();

	BAN::ErrorOr<void> feed_tokens(BAN::Vector<Token>&& tokens);
	BAN::ErrorOr<void> unget_token(Token&& token);

	BAN::ErrorOr<void> ask_input_tokens(BAN::StringView prompt, bool add_newline);

	BAN::ErrorOr<void> run(BAN::Vector<Token>&&);

	BAN::ErrorOr<CommandArgument::ArgumentPart> parse_backslash(bool is_quoted);
	BAN::ErrorOr<CommandArgument::ArgumentPart> parse_dollar();
	BAN::ErrorOr<CommandArgument::ArgumentPart> parse_single_quote();
	BAN::ErrorOr<CommandArgument> parse_argument();

	BAN::ErrorOr<SingleCommand> parse_single_command();
	BAN::ErrorOr<PipedCommand> parse_piped_command();
	BAN::ErrorOr<CommandTree> parse_command_tree();

private:
	Execute m_execute;

	Token m_eof_token { Token::Type::EOF_ };
	BAN::Queue<BAN::Vector<Token>> m_token_stream;
	InputFunction m_input_function;
};
