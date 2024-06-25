#include <kernel/ACPI/AML/Device.h>
#include <kernel/ACPI/AML/Integer.h>
#include <kernel/ACPI/AML/Method.h>
#include <kernel/ACPI/AML/Namespace.h>
#include <kernel/ACPI/AML/ParseContext.h>
#include <kernel/ACPI/AML/Region.h>
#include <kernel/ACPI/AML/String.h>
#include <kernel/Lock/LockGuard.h>

namespace Kernel::ACPI
{

	static BAN::RefPtr<AML::Namespace> s_root_namespace;
	static BAN::Vector<uint8_t> s_osi_aml_data;

	BAN::RefPtr<AML::Integer> AML::Integer::Constants::Zero;
	BAN::RefPtr<AML::Integer> AML::Integer::Constants::One;
	BAN::RefPtr<AML::Integer> AML::Integer::Constants::Ones;

	BAN::RefPtr<AML::Namespace> AML::Namespace::root_namespace()
	{
		ASSERT(s_root_namespace);
		return s_root_namespace;
	}

	void AML::Namespace::debug_print(int indent) const
	{
		LockGuard _(m_object_mutex);
		AML_DEBUG_PRINT_INDENT(indent);
		AML_DEBUG_PRINTLN("Namespace {} {", name);
		for_each_child(scope, [&](const auto&, const auto& child) {
			child->debug_print(indent + 1);
			AML_DEBUG_PRINTLN("");
		});
		AML_DEBUG_PRINT_INDENT(indent);
		AML_DEBUG_PRINT("}");
	}

	BAN::Optional<BAN::String> AML::Namespace::resolve_path(const AML::NameString& relative_base, const AML::NameString& relative_path, FindMode mode, bool check_existence) const
	{
		LockGuard _(m_object_mutex);

		// Base must be non-empty absolute path
		ASSERT(relative_base.prefix == "\\"_sv || relative_base.path.empty());

		// Do absolute path lookup
		if (!relative_path.prefix.empty() || relative_path.path.size() != 1 || mode == FindMode::ForceAbsolute)
		{
			BAN::String absolute_path;
			MUST(absolute_path.push_back('\\'));

			// Resolve root and parent references
			if (relative_path.prefix == "\\"_sv)
				;
			else
			{
				if (relative_path.prefix.size() > relative_base.path.size())
				{
					AML_ERROR("Trying to resolve parent of root object");
					return {};
				}
				for (size_t i = 0; i < relative_base.path.size() - relative_path.prefix.size(); i++)
				{
					MUST(absolute_path.append(relative_base.path[i].sv()));
					MUST(absolute_path.push_back('.'));
				}
			}

			// Append relative path
			for (const auto& seg : relative_path.path)
			{
				MUST(absolute_path.append(seg.sv()));
				MUST(absolute_path.push_back('.'));
			}

			if (absolute_path.back() == '.')
				absolute_path.pop_back();

			if (!check_existence || absolute_path == "\\"_sv || m_objects.contains(absolute_path))
				return absolute_path;
			return {};
		}


		// Resolve with namespace search rules (ACPI Spec 6.4 - Section 5.3)

		AML::NameSeg target_seg = relative_path.path.back();

		BAN::String last_match_path;
		BAN::String current_path;
		MUST(current_path.push_back('\\'));

		// Check root namespace
		{
			BAN::String tmp;
			MUST(tmp.append(current_path));
			MUST(tmp.append(target_seg.sv()));
			if (m_objects.contains(tmp))
				last_match_path = BAN::move(tmp);
		}

		// Check base base path
		for (const auto& seg : relative_base.path)
		{
			MUST(current_path.append(seg.sv()));
			MUST(current_path.push_back('.'));

			BAN::String tmp;
			MUST(tmp.append(current_path));
			MUST(tmp.append(target_seg.sv()));
			if (m_objects.contains(tmp))
				last_match_path = BAN::move(tmp);
		}

		if (!last_match_path.empty())
			return last_match_path;
		return {};
	}

	BAN::RefPtr<AML::NamedObject> AML::Namespace::find_object(const AML::NameString& relative_base, const AML::NameString& relative_path, FindMode mode)
	{
		LockGuard _(m_object_mutex);

		auto canonical_path = resolve_path(relative_base, relative_path, mode);
		if (!canonical_path.has_value())
			return nullptr;

		if (canonical_path->sv() == "\\"_sv)
			return this;

		auto it = m_objects.find(canonical_path.value());
		if (it == m_objects.end())
			return {};
		return it->value;
	}

	bool AML::Namespace::add_named_object(ParseContext& parse_context, const AML::NameString& object_path, BAN::RefPtr<NamedObject> object)
	{
		LockGuard _(m_object_mutex);

		ASSERT(!object_path.path.empty());
		ASSERT(object_path.path.back() == object->name);

		auto canonical_path = resolve_path(parse_context.scope, object_path, FindMode::ForceAbsolute, false);
		ASSERT(canonical_path.has_value());
		ASSERT(!canonical_path->empty());

		if (m_objects.contains(canonical_path.value()))
		{
			AML_ERROR("Object '{}' already exists", canonical_path.value());
			return false;
		}

		auto canonical_scope = AML::NameString(canonical_path.value());

		MUST(m_objects.insert(canonical_path.value(), object));
		if (object->is_scope())
		{
			auto* scope = static_cast<Scope*>(object.ptr());
			scope->scope = canonical_scope;
		}

		MUST(parse_context.created_objects.push_back(canonical_scope));

		return true;
	}

	bool AML::Namespace::remove_named_object(const AML::NameString& absolute_path)
	{
		LockGuard _(m_object_mutex);

		auto canonical_path = resolve_path({}, absolute_path, FindMode::ForceAbsolute);
		if (!canonical_path.has_value())
		{
			AML_ERROR("Trying to delete non-existent object '{}'", absolute_path);
			return false;
		}

		if (canonical_path->empty())
		{
			AML_ERROR("Trying to remove root namespace");
			return false;
		}

		ASSERT(m_objects.contains(canonical_path.value()));
		m_objects.remove(canonical_path.value());

		return true;
	}

	BAN::RefPtr<AML::Namespace> AML::Namespace::create_root_namespace()
	{
		ASSERT(!s_root_namespace);
		s_root_namespace = MUST(BAN::RefPtr<Namespace>::create(NameSeg("\\"_sv)));
		s_root_namespace->scope = AML::NameString("\\"_sv);

		Integer::Constants::Zero = MUST(BAN::RefPtr<Integer>::create(0, true));
		Integer::Constants::One = MUST(BAN::RefPtr<Integer>::create(1, true));
		Integer::Constants::Ones = MUST(BAN::RefPtr<Integer>::create(0xFFFFFFFFFFFFFFFF, true));

		AML::ParseContext context;
		context.scope = AML::NameString("\\"_sv);

		// Add predefined namespaces
#define ADD_PREDEFIED_NAMESPACE(NAME) \
			ASSERT(s_root_namespace->add_named_object(context, AML::NameString("\\" NAME), MUST(BAN::RefPtr<AML::Device>::create(NameSeg(NAME)))));
		ADD_PREDEFIED_NAMESPACE("_GPE"_sv);
		ADD_PREDEFIED_NAMESPACE("_PR"_sv);
		ADD_PREDEFIED_NAMESPACE("_SB"_sv);
		ADD_PREDEFIED_NAMESPACE("_SI"_sv);
		ADD_PREDEFIED_NAMESPACE("_TZ"_sv);
#undef ADD_PREDEFIED_NAMESPACE

		// Add \_OSI that returns true for Linux compatibility
		auto osi = MUST(BAN::RefPtr<AML::Method>::create(NameSeg("_OSI"_sv), 1, false, 0));
		osi->override_function = [](AML::ParseContext& context) -> BAN::RefPtr<AML::Node> {
			ASSERT(context.method_args[0]);
			auto arg = context.method_args[0]->evaluate();
			if (!arg || arg->type != AML::Node::Type::String)
			{
				AML_ERROR("Invalid _OSI argument");
				return {};
			}
			auto string = static_cast<AML::String*>(arg.ptr());
			return string->string_view() == "Linux"_sv ? AML::Integer::Constants::Ones : AML::Integer::Constants::Zero;
		};
		ASSERT(s_root_namespace->add_named_object(context, AML::NameString("\\_OSI"), osi));

		return s_root_namespace;
	}

	bool AML::Namespace::parse(const SDTHeader& header)
	{
		ASSERT(this == s_root_namespace.ptr());

		AML::ParseContext context;
		context.scope = AML::NameString("\\"_sv);
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
