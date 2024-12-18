#pragma once

#include <BAN/Hash.h>
#include <BAN/NoCopyMove.h>
#include <BAN/Vector.h>

namespace Kernel::ACPI::AML
{

	struct Scope
	{
		BAN_NON_COPYABLE(Scope);
	public:
		Scope() = default;
		Scope(Scope&& other) { *this = BAN::move(other); }
		Scope& operator=(Scope&& other) { parts = BAN::move(other.parts); return *this; }

		BAN::Vector<uint32_t> parts;

		BAN::ErrorOr<Scope> copy() const
		{
			Scope result;
			TRY(result.parts.reserve(parts.size()));
			for (uint32_t part : parts)
				TRY(result.parts.push_back(part));
			return result;
		}

		bool operator==(const Scope& other) const
		{
			if (parts.size() != other.parts.size())
				return false;
			for (size_t i = 0; i < parts.size(); i++)
				if (parts[i] != other.parts[i])
					return false;
			return true;
		}
	};

}

namespace BAN
{

	template<>
	struct hash<Kernel::ACPI::AML::Scope>
	{
		hash_t operator()(const Kernel::ACPI::AML::Scope& scope) const
		{
			hash_t hash { 0 };
			for (uint32_t part : scope.parts)
				hash ^= u32_hash(part);
			return hash;
		}
	};

}

namespace BAN::Formatter
{

	template<typename F>
	void print_argument(F putc, const Kernel::ACPI::AML::Scope& scope, const ValueFormat&)
	{
		putc('\\');
		for (size_t i = 0; i < scope.parts.size(); i++) {
			if (i != 0)
				putc('.');
			const char* name_seg = reinterpret_cast<const char*>(&scope.parts[i]);
			putc(name_seg[0]); putc(name_seg[1]); putc(name_seg[2]); putc(name_seg[3]);
		}
	}

}
