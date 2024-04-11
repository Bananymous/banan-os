#pragma once

#include <BAN/Optional.h>
#include <BAN/String.h>
#include <BAN/Vector.h>
#include <kernel/ACPI/AML/Bytes.h>
#include <kernel/ACPI/AML/Node.h>
#include <kernel/ACPI/AML/Utils.h>

namespace Kernel::ACPI::AML
{

	struct NameSeg
	{
		union {
			char chars[4];
			uint32_t u32;
		};

		NameSeg() = default;

		NameSeg(BAN::StringView name)
		{
			ASSERT(name.size() <= 4);
			for (size_t i = 0; i < name.size(); i++)
				chars[i] = static_cast<char>(name[i]);
			for (size_t i = name.size(); i < 4; i++)
				chars[i] = '_';
		}

		NameSeg(BAN::ConstByteSpan& aml_data)
		{
			ASSERT(aml_data.size() >= 4);
			for (size_t i = 0; i < 4; i++)
				chars[i] = static_cast<char>(aml_data[i]);
			aml_data = aml_data.slice(4);
		}

		static BAN::Optional<NameSeg> parse(BAN::ConstByteSpan& aml_data)
		{
			if (aml_data.size() < 4)
				return {};

			if (!is_lead_name_char(aml_data[0])
				|| !is_name_char(aml_data[1])
				|| !is_name_char(aml_data[2])
				|| !is_name_char(aml_data[3]))
				return {};

			return NameSeg(aml_data);
		}

		constexpr bool operator==(const NameSeg& other) const
		{
			return u32 == other.u32;
		}

		void debug_print() const
		{
			size_t len = 4;
			while (len > 0 && chars[len - 1] == '_')
				len--;
			for (size_t i = 0; i < len; i++)
				AML_DEBUG_PUTC(chars[i]);
		}
	};

	struct NameString
	{
		BAN::String prefix;
		BAN::Vector<NameSeg> path;

		NameString() = default;
		NameString(BAN::StringView str)
		{
			if (!str.empty() && str.front() == '\\')
			{
				MUST(prefix.push_back('\\'));
				str = str.substring(1);
			}
			else
			{
				while (str.size() > 0 && str.front() == '^')
				{
					MUST(prefix.push_back('^'));
					str = str.substring(1);
				}
			}

			while (!str.empty())
			{
				ASSERT(str[0] != '.');
				size_t len = 1;
				while (len < str.size() && str[len] != '.')
					len++;
				ASSERT(len <= 4);

				MUST(path.push_back(NameSeg(str.substring(0, len))));
				str = str.substring(len);

				if (!str.empty())
				{
					ASSERT(str[0] == '.');
					str = str.substring(1);
				}
			}
		}

		static bool can_parse(BAN::ConstByteSpan aml_data)
		{
			if (aml_data.size() == 0)
				return false;
			switch (static_cast<AML::Byte>(aml_data[0]))
			{
				case AML::Byte::RootChar:
				case AML::Byte::ParentPrefixChar:
				case AML::Byte::NullName:
				case AML::Byte::DualNamePrefix:
				case AML::Byte::MultiNamePrefix:
					return true;
				default:
					return is_lead_name_char(aml_data[0]);
			}
		}

		static BAN::Optional<NameString> parse(BAN::ConstByteSpan& aml_data)
		{
			if (aml_data.size() == 0)
				return {};

			NameString result;

			if (static_cast<AML::Byte>(aml_data[0]) == AML::Byte::RootChar)
			{
				MUST(result.prefix.push_back('\\'));
				aml_data = aml_data.slice(1);
			}
			else
			{
				while (aml_data.size() > 0 && static_cast<AML::Byte>(aml_data[0]) == AML::Byte::ParentPrefixChar)
				{
					MUST(result.prefix.push_back(aml_data[0]));
					aml_data = aml_data.slice(1);
				}
			}

			if (aml_data.size() == 0)
				return {};

			size_t name_count = 1;
			switch (static_cast<AML::Byte>(aml_data[0]))
			{
				case AML::Byte::NullName:
					name_count = 0;
					aml_data = aml_data.slice(1);
					break;
				case AML::Byte::DualNamePrefix:
					name_count = 2;
					aml_data = aml_data.slice(1);
					break;
				case AML::Byte::MultiNamePrefix:
					if (aml_data.size() < 2)
						return {};
					name_count = aml_data[1];
					aml_data = aml_data.slice(2);
					break;
				default:
					break;
			}

			for (size_t i = 0; i < name_count; i++)
			{
				auto name_seg = NameSeg::parse(aml_data);
				if (!name_seg.has_value())
					return {};
				MUST(result.path.push_back(name_seg.release_value()));
			}

			return result;
		}

		void debug_print() const
		{
			for (size_t i = 0; i < prefix.size(); i++)
				AML_DEBUG_PUTC(prefix[i]);
			if (!path.empty())
				path.front().debug_print();
			for (size_t i = 1; i < path.size(); i++)
			{
				AML_DEBUG_PUTC('.');
				path[i].debug_print();
			}
		}
	};

}

namespace BAN
{

	template<>
	struct hash<Kernel::ACPI::AML::NameSeg>
	{
		constexpr hash_t operator()(Kernel::ACPI::AML::NameSeg name) const
		{
			return hash<uint32_t>()(name.u32);
		}
	};

	template<typename F>
	void Formatter::print_argument(F putc, const Kernel::ACPI::AML::NameSeg& name_seg, const ValueFormat&)
	{
		size_t len = 4;
		while (len > 0 && name_seg.chars[len - 1] == '_')
			len--;
		for (size_t i = 0; i < len; i++)
			putc(name_seg.chars[i]);
	}

	template<typename F>
	void Formatter::print_argument(F putc, const Kernel::ACPI::AML::NameString& name_string, const ValueFormat&)
	{
		print_argument(putc, name_string.prefix, {});
		if (!name_string.path.empty())
			print_argument(putc, name_string.path.front(), {});
		for (size_t i = 1; i < name_string.path.size(); i++)
		{
			putc('.');
			print_argument(putc, name_string.path[i], {});
		}
	}

}
