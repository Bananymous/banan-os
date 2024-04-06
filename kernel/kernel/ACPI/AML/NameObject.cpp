#include <kernel/ACPI/AML/Bytes.h>
#include <kernel/ACPI/AML/NameObject.h>
#include <kernel/ACPI/AML/TermObject.h>

namespace Kernel::ACPI
{

	static constexpr bool is_lead_name_char(uint8_t ch)
	{
		return ('A' <= ch && ch <= 'Z') || ch == '_';
	}

	static constexpr bool is_name_char(uint8_t ch)
	{
		return is_lead_name_char(ch) || ('0' <= ch && ch <= '9');
	}


	// NameString

	bool AML::NameString::can_parse(BAN::ConstByteSpan span)
	{
		if (span.size() < 1)
			return false;
		if (span[0] == '\\' || span[0] == '^' || span[0] == 0x00)
			return true;
		if (static_cast<AML::Byte>(span[0]) == AML::Byte::DualNamePrefix)
			return true;
		if (static_cast<AML::Byte>(span[0]) == AML::Byte::MultiNamePrefix)
			return true;
		if (is_lead_name_char(span[0]))
			return true;
		return false;
	}

	BAN::Optional<AML::NameString> AML::NameString::parse(BAN::ConstByteSpan& span)
	{
		AML_DEBUG_PRINT_FN();
		ASSERT(can_parse(span));

		NameString name_string;

		if (span[0] == '\\')
		{
			MUST(name_string.prefix.push_back('\\'));
			span = span.slice(1);
		}
		else if (span[0] == '^')
		{
			while (span[0] == '^')
			{
				MUST(name_string.prefix.push_back('^'));
				span = span.slice(1);
			}
		}

		size_t name_count = 1;
		switch (span[0])
		{
			case 0x00:
				name_count = 0;
				span = span.slice(1);
				break;
			case static_cast<uint8_t>(AML::Byte::DualNamePrefix):
				name_count = 2;
				span = span.slice(1);
				break;
			case static_cast<uint8_t>(AML::Byte::MultiNamePrefix):
				name_count = span[1];
				span = span.slice(2);
				break;
		}

		if (span.size() < name_count * 4)
			return {};

		MUST(name_string.path.resize(name_count));

		for (size_t i = 0; i < name_count; i++)
		{
			if (!is_lead_name_char(span[0]) || !is_name_char(span[1]) || !is_name_char(span[2]) || !is_name_char(span[3]))
			{
				AML_DEBUG_ERROR("Invalid NameSeg {2H} {2H} {2H} {2H}", span[0], span[1], span[2], span[3]);
				ASSERT_NOT_REACHED();
				return {};
			}
			MUST(name_string.path[i].append(BAN::StringView(reinterpret_cast<const char*>(span.data()), 4)));
			while (name_string.path[i].back() == '_')
				name_string.path[i].pop_back();
			span = span.slice(4);
		}

		if constexpr(DUMP_AML)
		{
			BAN::String full_string;
			MUST(full_string.append(name_string.prefix));
			for (size_t i = 0; i < name_string.path.size(); i++)
			{
				if (i != 0)
					MUST(full_string.push_back('.'));
				MUST(full_string.append(name_string.path[i]));
			}
			AML_DEBUG_PRINT("'{}'", full_string);
		}

		return name_string;
	}


	// SimpleName

	bool AML::SimpleName::can_parse(BAN::ConstByteSpan span)
	{
		if (NameString::can_parse(span))
			return true;
		if (ArgObj::can_parse(span))
			return true;
		if (LocalObj::can_parse(span))
			return true;
		return false;
	}

	BAN::Optional<AML::SimpleName> AML::SimpleName::parse(BAN::ConstByteSpan& span)
	{
		AML_DEBUG_PRINT_FN();
		ASSERT(can_parse(span));
		AML_TRY_PARSE_IF_CAN(NameString);
		AML_TRY_PARSE_IF_CAN(ArgObj);
		AML_TRY_PARSE_IF_CAN(LocalObj);
		ASSERT_NOT_REACHED();
	}


	// SuperName

	bool AML::SuperName::can_parse(BAN::ConstByteSpan span)
	{
		if (SimpleName::can_parse(span))
			return true;
		if (DebugObj::can_parse(span))
			return true;
		if (ReferenceTypeOpcode::can_parse(span))
			return true;
		return false;
	}

	BAN::Optional<AML::SuperName> AML::SuperName::parse(BAN::ConstByteSpan& span)
	{
		AML_DEBUG_PRINT_FN();
		ASSERT(can_parse(span));

		AML_TRY_PARSE_IF_CAN(SimpleName);
		AML_TRY_PARSE_IF_CAN(DebugObj);

		ASSERT(ReferenceTypeOpcode::can_parse(span));
		auto opcode = ReferenceTypeOpcode::parse(span);
		if (!opcode.has_value())
			return {};
		return SuperName { .name = MUST(BAN::UniqPtr<ReferenceTypeOpcode>::create(opcode.release_value())) };
	}

}
