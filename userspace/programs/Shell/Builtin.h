#pragma once

#include <BAN/Function.h>
#include <BAN/HashMap.h>
#include <BAN/Iteration.h>
#include <BAN/NoCopyMove.h>
#include <BAN/String.h>

#include <stdio.h>

class Execute;

class Builtin
{
	BAN_NON_COPYABLE(Builtin);
	BAN_NON_MOVABLE(Builtin);
public:
	struct BuiltinCommand
	{
		using function_t = int (*)(Execute&, BAN::Span<const BAN::String>, FILE* fin, FILE* fout);

		function_t function { nullptr };
		bool immediate { false };

		BuiltinCommand(function_t function, bool immediate)
			: function(function)
			, immediate(immediate)
		{ }

		BAN::ErrorOr<int> execute(Execute&, BAN::Span<const BAN::String> arguments, int fd_in, int fd_out) const;
	};

public:
	Builtin() = default;
	static Builtin& get()
	{
		static Builtin s_instance;
		return s_instance;
	}

	void initialize();

	void for_each_builtin(BAN::Function<BAN::Iteration(BAN::StringView, const BuiltinCommand&)>) const;

	// return nullptr if not found
	const BuiltinCommand* find_builtin(const BAN::String& name) const;

private:
	BAN::HashMap<BAN::String, BuiltinCommand> m_builtin_commands;
};
