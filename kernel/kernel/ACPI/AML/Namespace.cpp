#include <kernel/ACPI/AML/Namespace.h>
#include <kernel/ACPI/AML/ParseContext.h>
#include <kernel/ACPI/AML/Region.h>

namespace Kernel::ACPI
{

	BAN::Optional<BAN::Vector<AML::NameSeg>> AML::Namespace::resolve_path(BAN::Span<const AML::NameSeg> parsing_scope, const AML::NameString& relative_path)
	{
		BAN::Vector<NameSeg> canonical_path;

		if (!relative_path.prefix.empty())
		{
			if (relative_path.prefix[0] == '\\')
				;
			else
			{
				if (parsing_scope.size() < relative_path.prefix.size())
				{
					AML_ERROR("Trying to resolve parent of root object");
					return {};
				}
				for (size_t i = 0; i < parsing_scope.size() - relative_path.prefix.size(); i++)
					MUST(canonical_path.push_back(parsing_scope[i]));
			}
		}
		else
		{
			for (auto seg : parsing_scope)
				MUST(canonical_path.push_back(seg));
		}

		for (const auto& seg : relative_path.path)
			MUST(canonical_path.push_back(seg));

		return canonical_path;
	}

	BAN::RefPtr<AML::NamedObject> AML::Namespace::find_object(BAN::Span<const AML::NameSeg> parsing_scope, const AML::NameString& relative_path)
	{
		auto canonical_path = resolve_path(parsing_scope, relative_path);
		if (!canonical_path.has_value())
			return nullptr;
		if (canonical_path->empty())
			return this;

		BAN::RefPtr<NamedObject> parent_object = this;

		for (const auto& seg : canonical_path.value())
		{
			if (!parent_object->is_scope())
			{
				AML_ERROR("Parent object is not a scope");
				return nullptr;
			}

			auto* parent_scope = static_cast<Scope*>(parent_object.ptr());

			auto it = parent_scope->objects.find(seg);
			if (it == parent_scope->objects.end())
				return nullptr;

			parent_object = it->value;
			ASSERT(parent_object);
		}

		return parent_object;
	}

	bool AML::Namespace::add_named_object(BAN::Span<const NameSeg> parsing_scope, const AML::NameString& object_path, BAN::RefPtr<NamedObject> object)
	{
		ASSERT(!object_path.path.empty());
		ASSERT(object_path.path.back() == object->name);

		auto parent_path = object_path;
		parent_path.path.pop_back();

		auto parent_object = find_object(parsing_scope, parent_path);
		if (!parent_object)
		{
			AML_ERROR("Parent object not found");
			return false;
		}

		if (!parent_object->is_scope())
		{
			AML_ERROR("Parent object is not a scope");
			return false;
		}

		auto* parent_scope = static_cast<Scope*>(parent_object.ptr());
		if (parent_scope->objects.contains(object->name))
		{
			AML_ERROR("Object already exists");
			return false;
		}

		MUST(parent_scope->objects.insert(object->name, object));
		return true;
	}

	BAN::RefPtr<AML::Namespace> AML::Namespace::parse(BAN::ConstByteSpan aml_data)
	{
		auto result = MUST(BAN::RefPtr<Namespace>::create());

		AML::ParseContext context;
		context.aml_data = aml_data;
		context.root_namespace = result.ptr();

		while (context.aml_data.size() > 0)
		{
			auto result = AML::parse_object(context);
			if (!result.success())
			{
				AML_ERROR("Failed to parse object");
				return {};
			}
		}

		return result;
	}

}
