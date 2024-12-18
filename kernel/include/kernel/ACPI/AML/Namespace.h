#pragma once

#include <BAN/Bitcast.h>
#include <BAN/ByteSpan.h>
#include <BAN/Function.h>
#include <BAN/HashMap.h>
#include <BAN/HashSet.h>
#include <BAN/Iteration.h>

#include <kernel/ACPI/AML/Node.h>

namespace Kernel::ACPI::AML
{

	struct Namespace
	{
		BAN_NON_COPYABLE(Namespace);
		BAN_NON_MOVABLE(Namespace);
	public:
		Namespace() = default;
		~Namespace();

		static BAN::ErrorOr<void> prepare_root_namespace();
		static Namespace& root_namespace();

		BAN::ErrorOr<void> post_load_initialize();

		BAN::ErrorOr<void> parse(BAN::ConstByteSpan);

		BAN::ErrorOr<Node> evaluate(BAN::StringView);

		// returns empty scope if object already exited
		BAN::ErrorOr<Scope> add_named_object(const Scope& scope, const NameString& name_string, Node&& node);
		BAN::ErrorOr<Scope> add_alias(const Scope& scope, const NameString& name_string, Reference* reference);

		BAN::ErrorOr<void> remove_named_object(const Scope& absolute_path);

		// node is nullptr if it is not found
		struct FindResult
		{
			Scope path;
			Reference* node { nullptr };
		};
		BAN::ErrorOr<FindResult> find_named_object(const Scope& scope, const NameString& name_string, bool force_absolute = false);

		BAN::ErrorOr<void> for_each_child(const Scope&, const BAN::Function<BAN::Iteration(BAN::StringView, Reference*)>&);
		BAN::ErrorOr<void> for_each_child(const Scope&, const BAN::Function<BAN::Iteration(const Scope&, Reference*)>&);

		BAN::ErrorOr<BAN::Vector<Scope>> find_device_with_eisa_id(BAN::StringView eisa_id);

	private:
		BAN::ErrorOr<Scope> resolve_path(const Scope& scope, const NameString& name_string);

		BAN::ErrorOr<void> initialize_scope(const Scope& scope);

		BAN::ErrorOr<void> opregion_call_reg(const Scope& scope, const Node& opregion);

		BAN::ErrorOr<uint64_t> evaluate_sta(const Scope& scope);
		BAN::ErrorOr<void> evaluate_ini(const Scope& scope);

		BAN::ErrorOr<void> initialize_op_regions();

	private:
		bool m_has_initialized_namespace { false };
		BAN::HashMap<Scope, Reference*> m_named_objects;
		BAN::HashMap<Scope, uint32_t> m_called_reg_bitmaps;
		BAN::HashSet<Scope> m_aliases;
	};

}
