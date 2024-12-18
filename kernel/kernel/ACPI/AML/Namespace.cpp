#include <BAN/Bitcast.h>

#include <kernel/ACPI/AML/Namespace.h>
#include <kernel/ACPI/AML/Node.h>
#include <kernel/ACPI/Headers.h>

#define STA_PRESENT  0x01
#define STA_FUNCTION 0x08

#include <ctype.h>

namespace Kernel::ACPI::AML
{

	static Namespace s_root_namespace;

	static constexpr BAN::StringView s_supported_osi_strings[] {
		"Windows 2000"_sv,
		"Windows 2001"_sv,
		"Windows 2001 SP1"_sv,
		"Windows 2001.1"_sv,
		"Windows 2001 SP2"_sv,
		"Windows 2001.1 SP1"_sv,
		"Windows 2006.1"_sv,
		"Windows 2006 SP1"_sv,
		"Windows 2006 SP2"_sv,
		"Windows 2009"_sv,
		"Windows 2012"_sv,
		"Windows 2013"_sv,
		"Windows 2015"_sv,
		"Windows 2016"_sv,
		"Windows 2017"_sv,
		"Windows 2017.2"_sv,
		"Windows 2018"_sv,
		"Windows 2018.2"_sv,
		"Windows 2019"_sv,
		"Extended Address Space Descriptor"_sv,
		// just to pass osi test from uACPI :D
		"AnotherTestString"_sv,
	};

	Namespace::~Namespace()
	{
		for (auto& [_, reference] : m_named_objects)
			if (--reference->ref_count == 0)
				delete reference;
	}

	BAN::ErrorOr<void> Namespace::prepare_root_namespace()
	{
		// https://uefi.org/htmlspecs/ACPI_Spec_6_4_html/05_ACPI_Software_Programming_Model/ACPI_Software_Programming_Model.html?highlight=predefined#predefined-root-namespaces

		const auto add_predefined_root_namespace =
			[](const char* name) -> BAN::ErrorOr<void>
			{
				Node predefined {};
				predefined.type = Node::Type::PredefinedScope;
				TRY(s_root_namespace.add_named_object({}, TRY(NameString::from_string(name)), BAN::move(predefined)));
				return {};
			};

		TRY(add_predefined_root_namespace("\\"));
		TRY(add_predefined_root_namespace("\\_GPE"));
		TRY(add_predefined_root_namespace("\\_PR_"));
		TRY(add_predefined_root_namespace("\\_SB_"));
		TRY(add_predefined_root_namespace("\\_SI_"));
		TRY(add_predefined_root_namespace("\\_TZ_"));

		{
			Node revision;
			revision.type = Node::Type::Integer;
			revision.as.integer.value = 2;
			TRY(s_root_namespace.add_named_object({}, TRY(NameString::from_string("_REV")), BAN::move(revision)));
		}

		{
			auto osi_string = TRY(NameString::from_string("\\_OSI"));

			Node method {};
			method.type = Node::Type::Method;
			new (method.as.method.storage) Kernel::Mutex();
			method.as.method.arg_count = 1;
			method.as.method.override_func =
				[](const BAN::Array<Reference*, 7>& args) -> BAN::ErrorOr<Node>
				{
					ASSERT(args[0]);

					if (args[0]->node.type != Node::Type::String)
					{
						dwarnln("_OSI called with {}", args[0]->node);
						return BAN::Error::from_errno(EINVAL);
					}

					Node result {};
					result.type = Node::Type::Integer;
					result.as.integer.value = 0;

					const auto arg0 = args[0]->node.as.str_buf->as_sv();
					for (auto supported : s_supported_osi_strings)
					{
						if (supported != arg0)
							continue;
						result.as.integer.value = 0xFFFFFFFFFFFFFFFF;
						break;
					}

					return result;
				};

			TRY(s_root_namespace.add_named_object({}, osi_string, BAN::move(method)));
		}

		{
			auto gl_string = TRY(NameString::from_string("\\_GL_"));

			Node mutex {};
			mutex.type = Node::Type::Mutex;
			mutex.as.mutex = new Mutex();
			mutex.as.mutex->ref_count = 1;
			mutex.as.mutex->sync_level = 0;
			mutex.as.mutex->global_lock = true;

			TRY(s_root_namespace.add_named_object({}, gl_string, BAN::move(mutex)));
		}

		return {};
	}

	Namespace& Namespace::root_namespace()
	{
		return s_root_namespace;
	}

	BAN::ErrorOr<uint64_t> Namespace::evaluate_sta(const Scope& scope)
	{
		auto [child_path, child_ref] = TRY(find_named_object(scope, TRY(NameString::from_string("_STA"_sv))));
		if (child_ref == nullptr)
			return 0x0F;
		return TRY(convert_node(TRY(evaluate_node(child_path, child_ref->node)), ConvInteger, sizeof(uint64_t))).as.integer.value;
	}

	BAN::ErrorOr<void> Namespace::evaluate_ini(const Scope& scope)
	{
		auto [child_path, child_ref] = TRY(find_named_object(scope, TRY(NameString::from_string("_INI"_sv))));
		if (child_ref == nullptr)
			return {};
		TRY(evaluate_node(child_path, child_ref->node));
		return {};
	}

	BAN::ErrorOr<void> Namespace::post_load_initialize()
	{
		BAN::Vector<Scope> to_init;
		TRY(to_init.push_back({}));

		while (!to_init.empty())
		{
			BAN::Vector<Scope> to_init_next;

			for (const Scope& current : to_init)
			{
				TRY(for_each_child(current,
					[&](const Scope& child_path, Reference* child_ref) -> BAN::Iteration
					{
						if (m_aliases.contains(child_path))
							return BAN::Iteration::Continue;

						switch (child_ref->node.type)
						{
							case Node::Type::Device:
							case Node::Type::Processor:
							case Node::Type::ThermalZone:
							case Node::Type::PredefinedScope:
								break;
							default:
								return BAN::Iteration::Continue;
						}

						auto sta_ret = evaluate_sta(child_path);
						if (sta_ret.is_error())
							return BAN::Iteration::Continue;

						if (sta_ret.value() & STA_PRESENT)
							(void)evaluate_ini(child_path);

						if ((sta_ret.value() & STA_PRESENT) || (sta_ret.value() & STA_FUNCTION))
						{
							auto child_path_copy = child_path.copy();
							if (!child_path_copy.is_error())
								(void)to_init_next.push_back(child_path_copy.release_value());
						}

						(void)for_each_child(current,
							[&](const Scope& opregion_path, Reference* opregion_ref) -> BAN::Iteration
							{
								if (opregion_ref->node.type == Node::Type::OpRegion)
									(void)opregion_call_reg(opregion_path, opregion_ref->node);
								return BAN::Iteration::Continue;
							}
						);

						return BAN::Iteration::Continue;
					}
				));
			}

			to_init = BAN::move(to_init_next);
		}

		m_has_initialized_namespace = true;

		if (auto ret = initialize_op_regions(); ret.is_error())
			dwarnln("Failed to initialize all opregions: {}", ret.error());

		return {};
	}

	BAN::ErrorOr<void> Namespace::initialize_op_regions()
	{
		for (const auto& [obj_path, obj_ref] : m_named_objects)
		{
			if (obj_ref->node.type != Node::Type::OpRegion)
				continue;
			// FIXME: if _REG adds stuff to namespace, iterators are invalidated
			(void)opregion_call_reg(obj_path, obj_ref->node);
		}

		return {};
	}

	BAN::ErrorOr<void> Namespace::opregion_call_reg(const Scope& scope, const Node& opregion)
	{
		ASSERT(opregion.type == Node::Type::OpRegion);

		const auto address_space = opregion.as.opregion.address_space;
		if (address_space == GAS::AddressSpaceID::SystemIO || address_space == GAS::AddressSpaceID::SystemMemory)
			return {};
		const uint32_t address_space_u32 = static_cast<uint32_t>(address_space);
		if (address_space_u32 >= 32)
			return {};
		const uint32_t mask = static_cast<uint32_t>(1) << address_space_u32;

		auto parent = TRY(scope.copy());
		parent.parts.pop_back();

		auto it = m_called_reg_bitmaps.find(parent);
		if (it != m_called_reg_bitmaps.end())
			if (it->value & mask)
				return {};

		auto [reg_path, reg_obj] = TRY(find_named_object(parent, TRY(NameString::from_string("_REG"_sv)), true));
		if (reg_obj != nullptr)
		{
			if (reg_obj->node.type != Node::Type::Method)
				dwarnln("{} is not an method", reg_path);
			else
			{
				if (reg_obj->node.as.method.arg_count != 2)
					dwarnln("{} takes {} arguments", reg_obj->node.as.method.arg_count);
				else
				{
					Reference arg0;
					arg0.node.type = Node::Type::Integer;
					arg0.node.as.integer.value = address_space_u32;
					arg0.ref_count = 2; // evaluate should not delete

					Reference arg1;
					arg1.node.type = Node::Type::Integer;
					arg1.node.as.integer.value = 1;
					arg1.ref_count = 2; // evaluate should not delete

					BAN::Array<Reference*, 7> args(nullptr);
					args[0] = &arg0;
					args[1] = &arg1;

					if (auto ret = method_call(reg_path, reg_obj->node, BAN::move(args)); ret.is_error())
						dwarnln("Failed to evaluate {}: {}", reg_path, ret.error());
				}
			}
		}

		if (it != m_called_reg_bitmaps.end())
			it->value |= mask;
		else
			TRY(m_called_reg_bitmaps.insert(BAN::move(parent), mask));

		return {};
	}

	BAN::ErrorOr<Scope> Namespace::resolve_path(const Scope& scope, const NameString& name_string)
	{
		Scope resolved_path;

		if (name_string.base == NameString::base_root)
		{
			TRY(resolved_path.parts.reserve(name_string.parts.size()));
			for (uint32_t part : name_string.parts)
				TRY(resolved_path.parts.push_back(part));
		}
		else
		{
			const uint32_t scope_segment_count =
				(name_string.base < scope.parts.size())
					? scope.parts.size() - name_string.base
					: 0;

			TRY(resolved_path.parts.reserve(scope_segment_count + name_string.parts.size()));
			for (size_t i = 0; i < scope_segment_count; i++)
				TRY(resolved_path.parts.push_back(scope.parts[i]));
			for (uint32_t part : name_string.parts)
				TRY(resolved_path.parts.push_back(part));
		}

		return resolved_path;
	}

	BAN::ErrorOr<Scope> Namespace::add_named_object(const Scope& scope, const NameString& name_string, Node&& node)
	{
		dprintln_if(AML_DUMP_FUNCTION_CALLS, "add_named_object('{}', '{}', {})", scope, name_string, node);

		auto resolved_path = TRY(resolve_path(scope, name_string));
		if (m_named_objects.contains(resolved_path))
			return Scope();

		auto* reference = new Reference();
		if (reference == nullptr)
			return BAN::Error::from_errno(ENOMEM);
		reference->node = BAN::move(node);
		reference->ref_count = 1;

		TRY(m_named_objects.insert(TRY(resolved_path.copy()), reference));

		if (m_has_initialized_namespace && reference->node.type == Node::Type::OpRegion)
			(void)opregion_call_reg(resolved_path, reference->node);

		return resolved_path;
	}

	BAN::ErrorOr<Scope> Namespace::add_alias(const Scope& scope, const NameString& name_string, Reference* reference)
	{
		dprintln_if(AML_DUMP_FUNCTION_CALLS, "add_alias('{}', '{}', {})", scope, name_string, reference->node);

		auto resolved_path = TRY(resolve_path(scope, name_string));
		if (m_named_objects.contains(resolved_path))
			return Scope();

		ASSERT(reference->ref_count >= 1);
		reference->ref_count++;

		TRY(m_named_objects.insert(TRY(resolved_path.copy()), reference));
		TRY(m_aliases.insert(TRY(resolved_path.copy())));

		return resolved_path;
	}

	BAN::ErrorOr<void> Namespace::remove_named_object(const Scope& absolute_path)
	{
		dprintln_if(AML_DUMP_FUNCTION_CALLS, "remove_named_object('{}')", absolute_path);

		auto it = m_named_objects.find(absolute_path);
		if (it == m_named_objects.end())
			return BAN::Error::from_errno(ENOENT);
		if (--it->value->ref_count == 0)
			delete it->value;
		m_named_objects.remove(it);
		return {};
	}

	BAN::ErrorOr<Namespace::FindResult> Namespace::find_named_object(const Scope& scope, const NameString& name_string, bool force_absolute)
	{
		dprintln_if(AML_DUMP_FUNCTION_CALLS, "find_named_object('{}', '{}')", scope, name_string);

		if (force_absolute || name_string.base != 0)
		{
			// Absolute path

			auto resolved_path = TRY(resolve_path(scope, name_string));
			auto it = m_named_objects.find(resolved_path);
			if (it != m_named_objects.end()) {
				return FindResult {
					.path = BAN::move(resolved_path),
					.node = it->value,
				};
			}

			return FindResult {
				.path = {},
				.node = nullptr,
			};
		}

		// Relative path

		Scope path_guess;
		TRY(path_guess.parts.reserve(scope.parts.size() + name_string.parts.size()));
		for (const auto& part : scope.parts)
			TRY(path_guess.parts.push_back(part));
		for (const auto& part : name_string.parts)
			TRY(path_guess.parts.push_back(part));

		auto it = m_named_objects.find(path_guess);
		if (it != m_named_objects.end()) {
			return FindResult {
				.path = BAN::move(path_guess),
				.node = it->value,
			};
		}

		for (size_t i = 0; i < scope.parts.size(); i++)
		{
			path_guess.parts.remove(scope.parts.size() - i - 1);
			auto it = m_named_objects.find(path_guess);
			if (it != m_named_objects.end()) {
				return FindResult {
					.path = BAN::move(path_guess),
					.node = it->value,
				};
			}
		}

		return FindResult {
			.path = {},
			.node = nullptr,
		};
	}

	BAN::ErrorOr<void> Namespace::for_each_child(const Scope& scope, const BAN::Function<BAN::Iteration(BAN::StringView, Reference*)>& callback)
	{
		for (const auto& [obj_path, obj_ref] : m_named_objects)
		{
			if (obj_path.parts.size() != scope.parts.size() + 1)
				continue;

			bool match = true;
			for (size_t i = 0; i < scope.parts.size() && match; i++)
				match = obj_path.parts[i] == scope.parts[i];
			if (!match)
				continue;

			auto name = BAN::StringView(reinterpret_cast<const char*>(&obj_path.parts.back()), 4);
			auto iteration = callback(name, obj_ref);
			if (iteration == BAN::Iteration::Break)
				break;
			ASSERT(iteration == BAN::Iteration::Continue);
		}

		return {};
	}

	BAN::ErrorOr<void> Namespace::for_each_child(const Scope& scope, const BAN::Function<BAN::Iteration(const Scope&, Reference*)>& callback)
	{
		for (const auto& [obj_path, obj_ref] : m_named_objects)
		{
			if (obj_path.parts.size() != scope.parts.size() + 1)
				continue;

			bool match = true;
			for (size_t i = 0; i < scope.parts.size() && match; i++)
				match = obj_path.parts[i] == scope.parts[i];
			if (!match)
				continue;

			auto iteration = callback(obj_path, obj_ref);
			if (iteration == BAN::Iteration::Break)
				break;
			ASSERT(iteration == BAN::Iteration::Continue);
		}

		return {};
	}

	static bool is_valid_eisa_id(BAN::StringView eisa_id)
	{
		if (eisa_id.size() != 7)
			return false;
		for (size_t i = 0; i < 3; i++)
			if (!isupper(eisa_id[i]))
				return false;
		for (size_t i = 3; i < 7; i++)
			if (!isxdigit(eisa_id[i]))
				return false;
		return true;
	}

	static uint32_t encode_eisa_id(BAN::StringView eisa_id)
	{
		constexpr auto char_to_hex =
			[](char ch) -> uint32_t
			{
				if (isdigit(ch))
					return ch - '0';
				return tolower(ch) - 'a' + 10;
			};

		uint16_t code = 0;
		code |= (eisa_id[2] - 0x40) << 0;
		code |= (eisa_id[1] - 0x40) << 5;
		code |= (eisa_id[0] - 0x40) << 10;

		uint32_t encoded = 0;
		encoded |= code >> 8;
		encoded |= (code & 0xFF) << 8;
		encoded |= (char_to_hex(eisa_id[3]) << 20) | (char_to_hex(eisa_id[4]) << 16);
		encoded |= (char_to_hex(eisa_id[5]) << 28) | (char_to_hex(eisa_id[6]) << 24);
		return encoded;
	}

	BAN::ErrorOr<BAN::Vector<Scope>> Namespace::find_device_with_eisa_id(BAN::StringView eisa_id)
	{
		if (!is_valid_eisa_id(eisa_id))
		{
			dwarnln("Invalid EISA id '{}'", eisa_id);
			return BAN::Error::from_errno(EINVAL);
		}

		const uint32_t encoded = encode_eisa_id(eisa_id);

		BAN::Vector<Scope> result;

		for (const auto& [obj_path, obj_ref] : m_named_objects)
		{
			if (obj_ref->node.type != Node::Type::Device)
				continue;

			auto [_, hid] = TRY(find_named_object(obj_path, TRY(NameString::from_string("_HID"_sv)), true));
			if (hid == nullptr)
				continue;

			uint32_t device_hid = 0;
			if (hid->node.type == Node::Type::Integer)
				device_hid = hid->node.as.integer.value;
			else if (hid->node.type == Node::Type::String && is_valid_eisa_id(hid->node.as.str_buf->as_sv()))
				device_hid = encode_eisa_id(hid->node.as.str_buf->as_sv());
			else
				continue;

			if (device_hid != encoded)
				continue;

			TRY(result.push_back(TRY(obj_path.copy())));
		}

		return result;
	}

	BAN::ErrorOr<Node> Namespace::evaluate(BAN::StringView path)
	{
		Scope root_scope;
		auto name_string = TRY(NameString::from_string(path));
		auto [object_path, object] = TRY(find_named_object(root_scope, name_string));
		if (object == nullptr)
			return BAN::Error::from_errno(ENOENT);
		return evaluate_node(object_path, object->node);
	}

	BAN::ErrorOr<void> Namespace::parse(BAN::ConstByteSpan aml_data)
	{
		if (aml_data.size() < sizeof(SDTHeader))
			return BAN::Error::from_errno(EINVAL);

		const auto& sdt_header = aml_data.as<const SDTHeader>();
		if (aml_data.size() < sdt_header.length)
			return BAN::Error::from_errno(EINVAL);

		aml_data = aml_data.slice(sizeof(SDTHeader));

		ParseContext context;
		context.aml_data = aml_data;
		TRY(context.allocate_locals());

		while (!context.aml_data.empty())
		{
			auto parse_result = parse_node_or_execution_flow(context);
			if (parse_result.is_error())
			{
				dwarnln("Failed to parse root namespace: {}", parse_result.error());
				return parse_result.release_error();
			}

			auto [execution_flow, node] = parse_result.release_value();
			if (execution_flow == ExecutionFlow::Normal)
				continue;
			if (execution_flow == ExecutionFlow::Return)
				break;
			dwarnln("Root namespace got execution flow {}", static_cast<int>(execution_flow));
			return BAN::Error::from_errno(EINVAL);
		}

		return {};
	}
}
