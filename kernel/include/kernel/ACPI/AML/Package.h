#pragma once

#include <kernel/ACPI/AML/Bytes.h>
#include <kernel/ACPI/AML/Node.h>
#include <kernel/ACPI/AML/ParseContext.h>
#include <kernel/ACPI/AML/Pkg.h>

namespace Kernel::ACPI::AML
{

	struct Package : public AML::Node
	{
		BAN::Vector<BAN::RefPtr<AML::Node>> elements;

		Package() : Node(Node::Type::Package) {}

		static ParseResult parse(AML::ParseContext& context)
		{
			ASSERT(context.aml_data.size() >= 1);
			ASSERT(static_cast<Byte>(context.aml_data[0]) == Byte::PackageOp);
			context.aml_data = context.aml_data.slice(1);

			auto package_pkg = AML::parse_pkg(context.aml_data);
			if (!package_pkg.has_value())
				return ParseResult::Failure;

			auto package_context = context;
			package_context.aml_data = package_pkg.value();

			if (package_pkg->size() < 1)
				return ParseResult::Failure;
			uint8_t num_elements = package_context.aml_data[0];
			package_context.aml_data = package_context.aml_data.slice(1);

			BAN::Vector<BAN::RefPtr<AML::Node>> elements;
			while (elements.size() < num_elements && package_context.aml_data.size() > 0)
			{
				auto element_result = AML::parse_object(package_context);
				if (!element_result.success())
					return ParseResult::Failure;
				MUST(elements.push_back(element_result.node()));
			}
			while (elements.size() < num_elements)
				MUST(elements.push_back(BAN::RefPtr<AML::Node>()));

			auto package = MUST(BAN::RefPtr<Package>::create());
			package->elements = BAN::move(elements);
			return ParseResult(package);
		}

		virtual void debug_print(int indent) const override
		{
			AML_DEBUG_PRINT_INDENT(indent);
			AML_DEBUG_PRINT("Package {");
			AML_DEBUG_PRINTLN("");
			for (const auto& element : elements)
			{
				element->debug_print(indent + 1);
				AML_DEBUG_PRINTLN("");
			}
			AML_DEBUG_PRINT_INDENT(indent);
			AML_DEBUG_PRINT("}");
		}
	};

}
