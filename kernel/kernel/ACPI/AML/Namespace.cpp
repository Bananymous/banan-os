#include <kernel/ACPI/AML/Namespace.h>
#include <kernel/ACPI/AML/ParseContext.h>
#include <kernel/ACPI/AML/Region.h>

namespace Kernel::ACPI
{

	static BAN::RefPtr<AML::Namespace> s_root_namespace;

	BAN::RefPtr<AML::Namespace> AML::Namespace::root_namespace()
	{
		ASSERT(s_root_namespace);
		return s_root_namespace;
	}

	BAN::Optional<AML::NameString> AML::Namespace::resolve_path(const AML::NameString& relative_base, const AML::NameString& relative_path)
	{
		// Base must be non-empty absolute path
		ASSERT(relative_base.prefix == "\\"sv || relative_base.path.empty());

		// Do absolute path lookup
		if (!relative_path.prefix.empty() || relative_path.path.size() != 1)
		{
			AML::NameString absolute_path;
			MUST(absolute_path.prefix.push_back('\\'));

			// Resolve root and parent references
			if (relative_path.prefix == "\\"sv)
				;
			else
			{
				if (relative_path.prefix.size() > relative_base.path.size())
				{
					AML_ERROR("Trying to resolve parent of root object");
					return {};
				}
				for (size_t i = 0; i < relative_base.path.size() - relative_path.prefix.size(); i++)
					MUST(absolute_path.path.push_back(relative_base.path[i]));
			}

			// Append relative path
			for (const auto& seg : relative_path.path)
				MUST(absolute_path.path.push_back(seg));

			// Validate path
			BAN::RefPtr<AML::NamedObject> current_node = this;
			for (const auto& seg : absolute_path.path)
			{
				if (!current_node->is_scope())
					return {};

				auto* current_scope = static_cast<AML::Scope*>(current_node.ptr());
				auto it = current_scope->objects.find(seg);
				if (it == current_scope->objects.end())
					return {};

				current_node = it->value;
			}
			return absolute_path;
		}


		// Resolve with namespace search rules (ACPI Spec 6.4 - Section 5.3)

		AML::NameString last_match_path;
		AML::NameSeg target_seg = relative_path.path.back();

		BAN::RefPtr<AML::Scope> current_scope = this;
		AML::NameString current_path;

		// Check root namespace
		{
			// If scope contains object with the same name as the segment, update last match
			if (current_scope->objects.contains(target_seg))
			{
				last_match_path = current_path;
				MUST(last_match_path.path.push_back(target_seg));
			}
		}

		// Check base base path
		for (const auto& seg : relative_base.path)
		{
			auto next_node = current_scope->objects[seg];
			ASSERT(next_node && next_node->is_scope());

			current_scope = static_cast<AML::Scope*>(next_node.ptr());
			MUST(current_path.path.push_back(seg));

			// If scope contains object with the same name as the segment, update last match
			if (current_scope->objects.contains(target_seg))
			{
				last_match_path = current_path;
				MUST(last_match_path.path.push_back(target_seg));
			}
		}

		if (!last_match_path.path.empty())
		{
			MUST(last_match_path.prefix.push_back('\\'));
			return last_match_path;
		}

		return {};
	}

	BAN::RefPtr<AML::NamedObject> AML::Namespace::find_object(const AML::NameString& relative_base, const AML::NameString& relative_path)
	{
		auto canonical_path = resolve_path(relative_base, relative_path);
		if (!canonical_path.has_value())
			return nullptr;
		if (canonical_path->path.empty())
			return this;

		BAN::RefPtr<NamedObject> node = this;
		for (const auto& seg : canonical_path->path)
		{
			// Resolve path validates that all nodes are scopes
			ASSERT(node->is_scope());
			node = static_cast<Scope*>(node.ptr())->objects[seg];
		}

		return node;
	}

	bool AML::Namespace::add_named_object(ParseContext& parse_context, const AML::NameString& object_path, BAN::RefPtr<NamedObject> object)
	{
		ASSERT(!object_path.path.empty());
		ASSERT(object_path.path.back() == object->name);

		auto parent_path = object_path;
		parent_path.path.pop_back();

		auto parent_object = find_object(parse_context.scope, parent_path);
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

		object->parent = parent_scope;

		MUST(parent_scope->objects.insert(object->name, object));

		auto canonical_scope = resolve_path(parse_context.scope, object_path);
		ASSERT(canonical_scope.has_value());
		if (object->is_scope())
		{
			auto* scope = static_cast<Scope*>(object.ptr());
			scope->scope = canonical_scope.value();
		}
		MUST(parse_context.created_objects.push_back(BAN::move(canonical_scope.release_value())));

		return true;
	}

	bool AML::Namespace::remove_named_object(const AML::NameString& absolute_path)
	{
		auto object = find_object({}, absolute_path);
		if (!object)
		{
			AML_ERROR("Object {} not found", absolute_path);
			return false;
		}

		if (object.ptr() == this)
		{
			AML_ERROR("Trying to remove root object");
			return false;
		}

		auto parent = object->parent;
		ASSERT(parent->is_scope());

		auto* parent_scope = static_cast<Scope*>(parent.ptr());
		parent_scope->objects.remove(object->name);

		return true;
	}

	BAN::RefPtr<AML::Namespace> AML::Namespace::create_root_namespace()
	{
		ASSERT(!s_root_namespace);
		s_root_namespace = MUST(BAN::RefPtr<Namespace>::create(NameSeg("\\"sv)));

		AML::ParseContext context;
		context.scope = AML::NameString("\\"sv);

		// Add predefined namespaces
#define ADD_PREDEFIED_NAMESPACE(NAME) \
			ASSERT(s_root_namespace->add_named_object(context, AML::NameString("\\" NAME), MUST(BAN::RefPtr<AML::Namespace>::create(NameSeg(NAME)))));
		ADD_PREDEFIED_NAMESPACE("_GPE"sv);
		ADD_PREDEFIED_NAMESPACE("_PR"sv);
		ADD_PREDEFIED_NAMESPACE("_SB"sv);
		ADD_PREDEFIED_NAMESPACE("_SI"sv);
		ADD_PREDEFIED_NAMESPACE("_TZ"sv);
#undef ADD_PREDEFIED_NAMESPACE

		return s_root_namespace;
	}

	bool AML::Namespace::parse(const SDTHeader& header)
	{
		ASSERT(this == s_root_namespace.ptr());

		dprintln("Parsing {}, {} bytes of AML", header, header.length);

		AML::ParseContext context;
		context.scope = AML::NameString("\\"sv);
		context.aml_data = BAN::ConstByteSpan(reinterpret_cast<const uint8_t*>(&header), header.length).slice(sizeof(header));

		while (context.aml_data.size() > 0)
		{
			auto result = AML::parse_object(context);
			if (!result.success())
			{
				AML_ERROR("Failed to parse object");
				return false;
			}
		}

		return true;
	}

}
