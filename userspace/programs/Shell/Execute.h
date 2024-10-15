#pragma once

#include "Builtin.h"
#include "CommandTypes.h"

#include <BAN/NoCopyMove.h>

class Execute
{
	BAN_NON_COPYABLE(Execute);
	BAN_NON_MOVABLE(Execute);
public:
	Execute() = default;

	BAN::ErrorOr<int> execute_command_sync(BAN::Span<const BAN::String> arguments, int fd_in, int fd_out);
	BAN::ErrorOr<void> execute_command(const SingleCommand&, int fd_in, int fd_out, bool background, pid_t pgrp = 0);
	BAN::ErrorOr<void> execute_command(const PipedCommand&);
	BAN::ErrorOr<void> execute_command(const CommandTree&);

	BAN::ErrorOr<void> source_script(BAN::StringView path);

	int last_background_pid() const { return m_last_background_pid; }
	int last_return_value() const { return m_last_return_value; }

private:
	struct InternalCommand
	{
		using Command = BAN::Variant<Builtin::BuiltinCommand, BAN::String>;

		enum class Type
		{
			Builtin,
			External,
		};

		struct Environment
		{
			BAN::String name;
			BAN::String value;
		};

		Command command;
		BAN::Span<const BAN::String> arguments;
		BAN::Span<const Environment> environments;
		int fd_in;
		int fd_out;
		bool background;
		pid_t pgrp;
	};

	struct ExecuteResult
	{
		pid_t pid;
		int exit_code;
	};

	BAN::ErrorOr<ExecuteResult> execute_command_no_wait(const InternalCommand& command);

private:
	int m_last_background_pid { 0 };
	int m_last_return_value { 0 };
};
