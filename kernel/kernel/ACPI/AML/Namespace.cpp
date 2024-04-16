#include <kernel/ACPI/AML/Device.h>
#include <kernel/ACPI/AML/Integer.h>
#include <kernel/ACPI/AML/Method.h>
#include <kernel/ACPI/AML/Namespace.h>
#include <kernel/ACPI/AML/ParseContext.h>
#include <kernel/ACPI/AML/Region.h>
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
		for (auto& [path, child] : m_objects)
		{
			child->debug_print(indent + 1);
			AML_DEBUG_PRINTLN("");
		}
		AML_DEBUG_PRINT_INDENT(indent);
		AML_DEBUG_PRINT("}");

	}

	BAN::Optional<BAN::String> AML::Namespace::resolve_path(const AML::NameString& relative_base, const AML::NameString& relative_path, bool allow_nonexistent)
	{
		LockGuard _(m_object_mutex);

		// Base must be non-empty absolute path
		ASSERT(relative_base.prefix == "\\"sv || relative_base.path.empty());

		// Do absolute path lookup
		if (!relative_path.prefix.empty() || relative_path.path.size() != 1 || allow_nonexistent)
		{
			BAN::String absolute_path;
			MUST(absolute_path.push_back('\\'));

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

			if (allow_nonexistent || m_objects.contains(absolute_path))
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

	BAN::RefPtr<AML::NamedObject> AML::Namespace::find_object(const AML::NameString& relative_base, const AML::NameString& relative_path)
	{
		LockGuard _(m_object_mutex);

		auto canonical_path = resolve_path(relative_base, relative_path);
		if (!canonical_path.has_value())
			return nullptr;

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

		auto canonical_path = resolve_path(parse_context.scope, object_path, true);
		ASSERT(canonical_path.has_value());

		if (canonical_path->empty())
		{
			AML_ERROR("Trying to add root namespace");
			return false;
		}

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

		auto canonical_path = resolve_path({}, absolute_path);
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
		s_root_namespace = MUST(BAN::RefPtr<Namespace>::create(NameSeg("\\"sv)));
		s_root_namespace->scope = AML::NameString("\\"sv);
		MUST(s_root_namespace->m_objects.insert("\\"sv, s_root_namespace));

		Integer::Constants::Zero = MUST(BAN::RefPtr<Integer>::create(0, true));
		Integer::Constants::One = MUST(BAN::RefPtr<Integer>::create(1, true));
		Integer::Constants::Ones = MUST(BAN::RefPtr<Integer>::create(0xFFFFFFFFFFFFFFFF, true));

		AML::ParseContext context;
		context.scope = AML::NameString("\\"sv);

		// Add predefined namespaces
#define ADD_PREDEFIED_NAMESPACE(NAME) \
			ASSERT(s_root_namespace->add_named_object(context, AML::NameString("\\" NAME), MUST(BAN::RefPtr<AML::Device>::create(NameSeg(NAME)))));
		ADD_PREDEFIED_NAMESPACE("_GPE"sv);
		ADD_PREDEFIED_NAMESPACE("_PR"sv);
		ADD_PREDEFIED_NAMESPACE("_SB"sv);
		ADD_PREDEFIED_NAMESPACE("_SI"sv);
		ADD_PREDEFIED_NAMESPACE("_TZ"sv);
#undef ADD_PREDEFIED_NAMESPACE

		// Add dummy \_OSI
		MUST(s_osi_aml_data.push_back(static_cast<uint8_t>(Byte::ReturnOp)));
		MUST(s_osi_aml_data.push_back(static_cast<uint8_t>(Byte::ZeroOp)));
		auto osi = MUST(BAN::RefPtr<AML::Method>::create(NameSeg("_OSI"sv), 1, false, 0));
		osi->term_list = s_osi_aml_data.span();
		ASSERT(s_root_namespace->add_named_object(context, AML::NameString("\\_OSI"), osi));

		return s_root_namespace;
	}

	bool AML::Namespace::parse(const SDTHeader& header)
	{
		ASSERT(this == s_root_namespace.ptr());

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
