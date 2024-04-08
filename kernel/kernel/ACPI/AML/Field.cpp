#include <kernel/ACPI/AML/Field.h>

namespace Kernel::ACPI
{

	template<typename Element>
	struct ParseFieldElementContext
	{
		AML::FieldRules										field_rules;
		uint64_t											field_bit_offset;
		BAN::ConstByteSpan									field_pkg;
		BAN::HashMap<AML::NameSeg, BAN::RefPtr<Element>>	elements;
	};

	template<typename Element>
	static bool parse_field_element(ParseFieldElementContext<Element>& context)
	{
		ASSERT(context.field_pkg.size() >= 1);
		switch (context.field_pkg[0])
		{
			case 0x00:
			{
				context.field_pkg = context.field_pkg.slice(1);

				auto reserved_length = AML::parse_pkg_length(context.field_pkg);
				if (!reserved_length.has_value())
				{
					AML_ERROR("Invalid FieldElement length for reserved field");
					return false;
				}
				AML::trim_pkg_length(context.field_pkg);

				context.field_bit_offset += reserved_length.value();
				return true;
			}
			case 0x01:
			case 0x02:
			case 0x03:
				AML_TODO("Field element {2H}", context.field_pkg[0]);
				return false;
			default:
			{
				auto element_name = AML::NameSeg::parse(context.field_pkg);
				if (!element_name.has_value())
				{
					AML_ERROR("Invalid FieldElement name for named field");
					return false;
				}

				auto element_length = AML::parse_pkg_length(context.field_pkg);
				if (!element_length.has_value())
				{
					AML_ERROR("Invalid FieldElement length for named field");
					return false;
				}
				AML::trim_pkg_length(context.field_pkg);

				if (context.elements.contains(element_name.value()))
				{
					AML_ERROR("Field element already exists");
					return false;
				}

				MUST(context.elements.emplace(
					element_name.value(),
					MUST(BAN::RefPtr<Element>::create(
						element_name.value(),
						context.field_bit_offset,
						element_length.value(),
						context.field_rules
					))
				));
				context.field_bit_offset += element_length.value();

				return true;
			}
		}
	}

	AML::ParseResult AML::Field::parse(ParseContext& context)
	{
		ASSERT(context.aml_data.size() >= 2);
		ASSERT(static_cast<AML::Byte>(context.aml_data[0]) == AML::Byte::ExtOpPrefix);
		ASSERT(static_cast<AML::ExtOp>(context.aml_data[1]) == AML::ExtOp::FieldOp);
		context.aml_data = context.aml_data.slice(2);

		auto opt_field_pkg = AML::parse_pkg(context.aml_data);
		if (!opt_field_pkg.has_value())
			return ParseResult::Failure;
		auto field_pkg = opt_field_pkg.release_value();

		auto name_string = NameString::parse(field_pkg);
		if (!name_string.has_value())
			return ParseResult::Failure;

		auto op_region = context.root_namespace->find_object(context.scope.span(), name_string.value());
		if (!op_region || op_region->type != AML::Node::Type::OpRegion)
		{
			AML_ERROR("Field RegionName does not name a valid OpRegion");
			return ParseResult::Failure;
		}

		if (field_pkg.size() < 1)
			return ParseResult::Failure;
		auto field_flags = field_pkg[0];
		field_pkg = field_pkg.slice(1);

		ParseFieldElementContext<FieldElement> field_context;
		field_context.field_rules.access_type = static_cast<FieldRules::AccessType>(field_flags & 0x0F);
		field_context.field_rules.lock_rule = static_cast<FieldRules::LockRule>((field_flags >> 4) & 0x01);
		field_context.field_rules.update_rule = static_cast<FieldRules::UpdateRule>((field_flags >> 5) & 0x03);
		field_context.field_bit_offset = 0;
		field_context.field_pkg = field_pkg;
		while (field_context.field_pkg.size() > 0)
			if (!parse_field_element(field_context))
				return ParseResult::Failure;

		for (auto& [_, element] : field_context.elements)
		{
			element->op_region = static_cast<OpRegion*>(op_region.ptr());

			NameString element_name;
			MUST(element_name.path.push_back(element->name));
			if (!context.root_namespace->add_named_object(context.scope.span(), element_name, element))
				return ParseResult::Failure;

#if AML_DEBUG_LEVEL >= 2
			element->debug_print(0);
			AML_DEBUG_PRINTLN("");
#endif
		}

		return ParseResult::Success;
	}

	void AML::FieldElement::debug_print(int indent) const
	{
		AML_DEBUG_PRINT_INDENT(indent);
		AML_DEBUG_PRINT("FieldElement ");
		name.debug_print();
		AML_DEBUG_PRINT("({}, offset {}, OpRegion ", bit_count, bit_offset);
		op_region->name.debug_print();
		AML_DEBUG_PRINT(")");
	}

	AML::ParseResult AML::IndexField::parse(ParseContext& context)
	{
		ASSERT(context.aml_data.size() >= 2);
		ASSERT(static_cast<AML::Byte>(context.aml_data[0]) == AML::Byte::ExtOpPrefix);
		ASSERT(static_cast<AML::ExtOp>(context.aml_data[1]) == AML::ExtOp::IndexFieldOp);
		context.aml_data = context.aml_data.slice(2);

		auto opt_field_pkg = AML::parse_pkg(context.aml_data);
		if (!opt_field_pkg.has_value())
			return ParseResult::Failure;
		auto field_pkg = opt_field_pkg.release_value();

		auto index_field_element_name = NameString::parse(field_pkg);
		if (!index_field_element_name.has_value())
			return ParseResult::Failure;
		auto index_field_element = context.root_namespace->find_object(context.scope.span(), index_field_element_name.value());
		if (!index_field_element || index_field_element->type != AML::Node::Type::FieldElement)
		{
			AML_ERROR("IndexField IndexName does not name a valid FieldElement");
			return ParseResult::Failure;
		}

		auto data_field_element_name = NameString::parse(field_pkg);
		if (!data_field_element_name.has_value())
			return ParseResult::Failure;
		auto data_field_element = context.root_namespace->find_object(context.scope.span(), data_field_element_name.value());
		if (!data_field_element || data_field_element->type != AML::Node::Type::FieldElement)
		{
			AML_ERROR("IndexField DataName does not name a valid FieldElement");
			return ParseResult::Failure;
		}

		if (field_pkg.size() < 1)
			return ParseResult::Failure;
		auto field_flags = field_pkg[0];
		field_pkg = field_pkg.slice(1);

		ParseFieldElementContext<IndexFieldElement> field_context;
		field_context.field_rules.access_type = static_cast<FieldRules::AccessType>(field_flags & 0x0F);
		field_context.field_rules.lock_rule = static_cast<FieldRules::LockRule>((field_flags >> 4) & 0x01);
		field_context.field_rules.update_rule = static_cast<FieldRules::UpdateRule>((field_flags >> 5) & 0x03);
		field_context.field_bit_offset = 0;
		field_context.field_pkg = field_pkg;
		while (field_context.field_pkg.size() > 0)
			if (!parse_field_element(field_context))
				return ParseResult::Failure;

		for (auto& [_, element] : field_context.elements)
		{
			element->index_element = static_cast<FieldElement*>(index_field_element.ptr());
			element->data_element = static_cast<FieldElement*>(data_field_element.ptr());

			NameString element_name;
			MUST(element_name.path.push_back(element->name));
			if (!context.root_namespace->add_named_object(context.scope.span(), element_name, element))
				return ParseResult::Failure;

#if AML_DEBUG_LEVEL >= 2
			element->debug_print(0);
			AML_DEBUG_PRINTLN("");
#endif
		}

		return AML::ParseResult::Success;
	}

	void AML::IndexFieldElement::debug_print(int indent) const
	{
		AML_DEBUG_PRINT_INDENT(indent);
		AML_DEBUG_PRINT("IndexFieldElement ");
		name.debug_print();
		AML_DEBUG_PRINT("({}, offset {}, IndexName ", bit_count, bit_offset);
		index_element->name.debug_print();
		AML_DEBUG_PRINT(", DataName ");
		data_element->name.debug_print();
		AML_DEBUG_PRINT(")");
	}

}
