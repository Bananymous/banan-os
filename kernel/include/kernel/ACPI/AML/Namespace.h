#pragma once

#include <kernel/ACPI/AML/Scope.h>
#include <kernel/Lock/Mutex.h>

namespace Kernel::ACPI::AML
{

	struct Namespace : public AML::Scope
	{
		Mutex global_lock;

		static BAN::RefPtr<AML::Namespace> root_namespace();

		Namespace(NameSeg name) : AML::Scope(Node::Type::Namespace, name) {}

		static BAN::RefPtr<Namespace> parse(BAN::ConstByteSpan aml);

		BAN::Optional<AML::NameString> resolve_path(const AML::NameString& relative_base, const AML::NameString& relative_path);

		// Find an object in the namespace. Returns nullptr if the object is not found.
		BAN::RefPtr<NamedObject> find_object(const AML::NameString& relative_base, const AML::NameString& relative_path);

		// Add an object to the namespace. Returns false if the parent object could not be added.
		bool add_named_object(ParseContext&, const AML::NameString& object_path, BAN::RefPtr<NamedObject> object);

		// Remove an object from the namespace. Returns false if the object could not be removed.
		bool remove_named_object(const AML::NameString& absolute_path);
	};

}
