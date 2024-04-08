#pragma once

#include <kernel/ACPI/AML/Scope.h>

namespace Kernel::ACPI::AML
{

	struct Namespace : public AML::Scope
	{
		Namespace() : AML::Scope(NameSeg("\\"sv)) {}

		static BAN::RefPtr<Namespace> parse(BAN::ConstByteSpan aml);

		BAN::Optional<BAN::Vector<AML::NameSeg>> resolve_path(BAN::Span<const AML::NameSeg> parsing_scope, const AML::NameString& relative_path);

		// Find an object in the namespace. Returns nullptr if the object is not found.
		BAN::RefPtr<NamedObject> find_object(BAN::Span<const AML::NameSeg> parsing_scope, const AML::NameString& relative_path);

		// Add an object to the namespace. Returns false if the parent object could not be added.
		bool add_named_object(BAN::Span<const AML::NameSeg> parsing_scope, const AML::NameString& object_path, BAN::RefPtr<NamedObject> object);
	};

}
