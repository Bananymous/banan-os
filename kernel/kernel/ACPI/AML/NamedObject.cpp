#include <kernel/ACPI/AML/NamedObject.h>
#include <kernel/ACPI/AML/ParseContext.h>

namespace Kernel::ACPI
{

	AML::ParseResult AML::Name::parse(ParseContext& context)
	{
		ASSERT(context.aml_data.size() >= 1);
		ASSERT(static_cast<Byte>(context.aml_data[0]) == Byte::NameOp);
		context.aml_data = context.aml_data.slice(1);

		auto name_string = AML::NameString::parse(context.aml_data);
		if (!name_string.has_value())
			return ParseResult::Failure;

		auto object = AML::parse_object(context);
		if (!object.success())
			return ParseResult::Failure;

		auto name = MUST(BAN::RefPtr<Name>::create(name_string.value().path.back(), object.node()));
		if (!context.root_namespace->add_named_object(context.scope.span(), name_string.value(), name))
			return ParseResult::Failure;

#if AML_DEBUG_LEVEL >= 2
		name->debug_print(0);
		AML_DEBUG_PRINTLN("");
#endif

		return ParseResult::Success;
	}

	void AML::Name::debug_print(int indent) const
	{
		AML_DEBUG_PRINT_INDENT(indent);
		AML_DEBUG_PRINT("Name ");
		name.debug_print();
		AML_DEBUG_PRINTLN(" {");
		object->debug_print(indent + 1);
		AML_DEBUG_PRINTLN("");
		AML_DEBUG_PRINT_INDENT(indent);
		AML_DEBUG_PRINT("}");
	}

}
