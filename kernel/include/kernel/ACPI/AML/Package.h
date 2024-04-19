#pragma once

#include <kernel/ACPI/AML/Bytes.h>
#include <kernel/ACPI/AML/Node.h>
#include <kernel/ACPI/AML/ParseContext.h>
#include <kernel/ACPI/AML/Pkg.h>

namespace Kernel::ACPI::AML
{

	struct PackageElement;

	struct Package : public AML::Node
	{
		BAN::Vector<BAN::RefPtr<PackageElement>> elements;
		AML::NameString scope;

		Package(AML::NameString scope)
			: Node(Node::Type::Package)
			, elements(BAN::move(elements))
			, scope(scope)
		{}

		BAN::RefPtr<AML::Node> evaluate() override
		{
			return this;
		}

		static ParseResult parse(AML::ParseContext& context);
		virtual void debug_print(int indent) const override;
	};

	struct PackageElement : public AML::Node
	{
		BAN::RefPtr<AML::Package> parent;
		BAN::RefPtr<AML::Node> element;
		AML::NameString unresolved_name;
		bool resolved = false;
		bool initialized = false;

		PackageElement(BAN::RefPtr<AML::Package> parent, BAN::RefPtr<AML::Node> element)
			: Node(Node::Type::PackageElement)
			, parent(parent)
			, element(element)
		{
			ASSERT(element);
			resolved = true;
			initialized = true;
		}

		PackageElement(BAN::RefPtr<AML::Package> parent, AML::NameString unresolved_name)
			: Node(Node::Type::PackageElement)
			, parent(parent)
			, unresolved_name(unresolved_name)
		{
			resolved = false;
			initialized = true;
		}

		PackageElement(BAN::RefPtr<AML::Package> parent)
			: Node(Node::Type::PackageElement)
			, parent(parent)
			, unresolved_name(unresolved_name)
		{
			resolved = false;
			initialized = false;
		}

		BAN::RefPtr<AML::Node> evaluate() override
		{
			if (!initialized)
			{
				AML_ERROR("Trying to evaluate uninitialized PackageElement");
				return {};
			}
			if (!resolved)
			{
				auto object = Namespace::root_namespace()->find_object(parent->scope, unresolved_name, Namespace::FindMode::Normal);
				if (!object)
				{
					AML_ERROR("Failed to resolve reference {} in package {}", unresolved_name, parent->scope);
					return {};
				}
				element = object;
				resolved = true;
			}
			return element->evaluate();
		}

		static ParseResult parse(AML::ParseContext& context, BAN::RefPtr<AML::Package> package)
		{
			BAN::RefPtr<AML::PackageElement> element;
			if (context.aml_data[0] != 0x00 && AML::NameString::can_parse(context.aml_data))
			{
				auto name = AML::NameString::parse(context.aml_data);
				if (!name.has_value())
					return ParseResult::Failure;
				element = MUST(BAN::RefPtr<PackageElement>::create(package, name.value()));
			}
			else
			{
				auto element_result = AML::parse_object(context);
				if (!element_result.success())
					return ParseResult::Failure;
				element = MUST(BAN::RefPtr<PackageElement>::create(package, element_result.node()));
			}
			return ParseResult(element);
		}

		virtual void debug_print(int indent) const override
		{
			AML_DEBUG_PRINT_INDENT(indent);
			AML_DEBUG_PRINTLN("PackageElement {");
			if (!initialized)
			{
				AML_DEBUG_PRINT_INDENT(indent + 1);
				AML_DEBUG_PRINT("Uninitialized");
			}
			else if (!resolved)
			{
				AML_DEBUG_PRINT_INDENT(indent + 1);
				AML_DEBUG_PRINT("Unresolved {}", unresolved_name);
			}
			else
			{
				element->debug_print(indent + 1);
			}
			AML_DEBUG_PRINTLN("");
			AML_DEBUG_PRINT_INDENT(indent);
			AML_DEBUG_PRINT("}");
		}
	};

}
