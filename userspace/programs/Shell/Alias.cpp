#include "Alias.h"

BAN::ErrorOr<void> Alias::set_alias(BAN::StringView name, BAN::StringView value)
{
	TRY(m_aliases.insert_or_assign(
		TRY(BAN::String::formatted("{}", name)),
		TRY(BAN::String::formatted("{}", value))
	));
	return {};
}

BAN::Optional<BAN::StringView> Alias::get_alias(const BAN::String& name) const
{
	auto it = m_aliases.find(name);
	if (it == m_aliases.end())
		return {};
	return it->value.sv();
}


void Alias::for_each_alias(BAN::Function<BAN::Iteration(BAN::StringView, BAN::StringView)> callback) const
{
	for (const auto& [name, value] : m_aliases)
	{
		switch (callback(name.sv(), value.sv()))
		{
			case BAN::Iteration::Break:
				break;
			case BAN::Iteration::Continue:
				continue;;
		}
		break;
	}
}
