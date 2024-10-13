#pragma once

#include <BAN/Function.h>
#include <BAN/HashMap.h>
#include <BAN/Iteration.h>
#include <BAN/NoCopyMove.h>
#include <BAN/String.h>

class Alias
{
	BAN_NON_COPYABLE(Alias);
	BAN_NON_MOVABLE(Alias);
public:
	Alias() = default;
	static Alias& get()
	{
		static Alias s_instance;
		return s_instance;
	}

	BAN::ErrorOr<void> set_alias(BAN::StringView name, BAN::StringView value);

	// NOTE: `const BAN::String&` instead of `BAN::StringView` to avoid BAN::String construction
	//       for hashmap accesses
	BAN::Optional<BAN::StringView> get_alias(const BAN::String& name) const;

	void for_each_alias(BAN::Function<BAN::Iteration(BAN::StringView, BAN::StringView)>) const;

private:
	BAN::HashMap<BAN::String, BAN::String> m_aliases;
};
