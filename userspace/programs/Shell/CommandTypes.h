#pragma once

#include <BAN/String.h>

#define COMMAND_GET_MACRO(_0, _1, _2, NAME, ...) NAME

#define COMMAND_MOVE_0(class) \
	class(class&& o) { } \
	class& operator=(class&& o) { }
#define COMMAND_MOVE_1(class, var) \
	class(class&& o) { var = BAN::move(o.var); } \
	class& operator=(class&& o) { var = BAN::move(o.var); return *this; }
#define COMMAND_MOVE_2(class, var1, var2) \
	class(class&& o) { var1 = BAN::move(o.var1); var2 = BAN::move(o.var2); } \
	class& operator=(class&& o) { var1 = BAN::move(o.var1); var2 = BAN::move(o.var2); return *this; }
#define COMMAND_MOVE(class, ...) COMMAND_GET_MACRO(_0 __VA_OPT__(,) __VA_ARGS__, COMMAND_MOVE_2, COMMAND_MOVE_1, COMMAND_MOVE_0)(class, __VA_ARGS__)

#define COMMAND_RULE5(class, ...) \
	class() = default; \
	class(const class&) = delete; \
	class& operator=(const class&) = delete; \
	COMMAND_MOVE(class, __VA_ARGS__)

struct CommandTree;
class Execute;

struct FixedString
{
	COMMAND_RULE5(FixedString, value);
	BAN::String value;
};

struct EnvironmentVariable
{
	COMMAND_RULE5(EnvironmentVariable, value);
	BAN::String value;
};

struct BuiltinVariable
{
	COMMAND_RULE5(BuiltinVariable, value);
	BAN::String value;
};

struct CommandArgument
{
	using ArgumentPart =
		BAN::Variant<
			FixedString,
			EnvironmentVariable,
			BuiltinVariable,
			CommandTree
		>;

	BAN::ErrorOr<BAN::String> evaluate(Execute& execute) const;

	COMMAND_RULE5(CommandArgument, parts);
	BAN::Vector<ArgumentPart> parts;
};

struct SingleCommand
{
	BAN::ErrorOr<BAN::Vector<BAN::String>> evaluate_arguments(Execute& execute) const;

	COMMAND_RULE5(SingleCommand, arguments);
	BAN::Vector<CommandArgument> arguments;
};

struct PipedCommand
{
	COMMAND_RULE5(PipedCommand, commands, background);
	BAN::Vector<SingleCommand> commands;
	bool background { false };
};

struct ConditionalCommand
{
	enum class Condition
	{
		Always,
		OnFailure,
		OnSuccess,
	};

	COMMAND_RULE5(ConditionalCommand, command, condition);
	PipedCommand command;
	Condition condition { Condition::Always };
};

struct CommandTree
{
	COMMAND_RULE5(CommandTree, commands);
	BAN::Vector<ConditionalCommand> commands;
};

#undef COMMAND_GET_MACRO
#undef COMMAND_MOVE_0
#undef COMMAND_MOVE_1
#undef COMMAND_MOVE_2
#undef COMMAND_MOVE
#undef COMMAND_RULE5
