#pragma once

#include <kernel/ACPI/AML/NamedObject.h>
#include <kernel/ACPI/AML/Names.h>

namespace Kernel::ACPI::AML
{

	struct Scope : public AML::NamedObject
	{
		AML::NameString scope;

		Scope(Node::Type type, NameSeg name)
			: NamedObject(type, name)
		{}

		virtual bool is_scope() const override { return true; }

		static ParseResult parse(ParseContext& context);

	protected:
		ParseResult enter_context_and_parse_term_list(ParseContext& outer_context, const AML::NameString& name, BAN::ConstByteSpan aml_data);
	};

	bool initialize_scope(BAN::RefPtr<Scope> scope);

}
