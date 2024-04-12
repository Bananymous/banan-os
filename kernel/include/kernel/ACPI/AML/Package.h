#pragma once

#include <kernel/ACPI/AML/Bytes.h>
#include <kernel/ACPI/AML/Node.h>
#include <kernel/ACPI/AML/ParseContext.h>
#include <kernel/ACPI/AML/Pkg.h>

namespace Kernel::ACPI::AML
{

	struct Package : public AML::Node
	{
		struct UnresolvedReference
		{
			AML::NameString name;
			size_t index;
		};
		BAN::Vector<UnresolvedReference> unresolved_references;
		AML::NameString scope; // Used for resolving references

		BAN::Vector<BAN::RefPtr<AML::Node>> elements;

		Package(BAN::Vector<BAN::RefPtr<AML::Node>>&& elements, BAN::Vector<UnresolvedReference>&& unresolved_references, AML::NameString scope)
			: Node(Node::Type::Package)
			, elements(BAN::move(elements))
			, unresolved_references(BAN::move(unresolved_references))
			, scope(scope)
		{}

		BAN::RefPtr<AML::Node> evaluate() override
		{
			// resolve references
			for (auto& reference : unresolved_references)
			{
				auto object = Namespace::root_namespace()->find_object(scope, reference.name);
				if (!object)
				{
					AML_ERROR("Failed to resolve reference {} in package", reference.name);
					return {};
				}
				ASSERT(!elements[reference.index]);
				elements[reference.index] = object;
			}
			unresolved_references.clear();

			return this;
		}

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
			BAN::Vector<UnresolvedReference> unresolved_references;
			while (elements.size() < num_elements && package_context.aml_data.size() > 0)
			{
				BAN::RefPtr<AML::Node> element;

				// Store name strings as references
				if (package_context.aml_data[0] != 0x00 && AML::NameString::can_parse(package_context.aml_data))
				{
					auto name = AML::NameString::parse(package_context.aml_data);
					if (!name.has_value())
						return ParseResult::Failure;
					MUST(unresolved_references.push_back(UnresolvedReference { .name = name.value(), .index = elements.size() }));
				}
				else
				{
					auto element_result = AML::parse_object(package_context);
					if (!element_result.success())
						return ParseResult::Failure;
					element = element_result.node();
				}

				MUST(elements.push_back(element));
			}
			while (elements.size() < num_elements)
				MUST(elements.push_back(BAN::RefPtr<AML::Node>()));

			auto package = MUST(BAN::RefPtr<Package>::create(BAN::move(elements), BAN::move(unresolved_references), context.scope));
			return ParseResult(package);
		}

		virtual void debug_print(int indent) const override
		{
			AML_DEBUG_PRINT_INDENT(indent);
			AML_DEBUG_PRINT("Package {");
			AML_DEBUG_PRINTLN("");
			for (const auto& element : elements)
			{
				AML_DEBUG_PRINT_INDENT(indent + 1);
				if (element)
					element->debug_print(0);
				else
					AML_DEBUG_PRINT("Uninitialized");
				AML_DEBUG_PRINTLN("");
			}
			AML_DEBUG_PRINT_INDENT(indent);
			AML_DEBUG_PRINT("}");
		}
	};

}
