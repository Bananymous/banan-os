#include <kernel/ACPI/AML/Package.h>

namespace Kernel::ACPI
{

	AML::ParseResult AML::Package::parse(AML::ParseContext& context)
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

		auto package = MUST(BAN::RefPtr<Package>::create(context.scope));
		while (package->elements.size() < num_elements && package_context.aml_data.size() > 0)
		{
			auto element_result = PackageElement::parse(package_context, package);
			if (!element_result.success())
				return ParseResult::Failure;
			ASSERT(element_result.node() && element_result.node()->type == Node::Type::PackageElement);
			auto element = static_cast<PackageElement*>(element_result.node().ptr());
			MUST(package->elements.push_back(element));
		}
		while (package->elements.size() < num_elements)
		{
			auto uninitialized = MUST(BAN::RefPtr<PackageElement>::create(package));
			MUST(package->elements.push_back(uninitialized));
		}

		return ParseResult(package);
	}

	void AML::Package::debug_print(int indent) const
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

}
