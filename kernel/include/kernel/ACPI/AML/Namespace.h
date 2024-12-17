#pragma once

#include <BAN/Bitcast.h>
#include <BAN/ByteSpan.h>
#include <BAN/Function.h>
#include <BAN/HashMap.h>
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

		static BAN::ErrorOr<void> initialize_root_namespace();
		static Namespace& root_namespace();

		// this has to be called after initalizing ACPI namespace
		BAN::ErrorOr<void> initalize_op_regions();

		BAN::ErrorOr<void> parse(BAN::ConstByteSpan);

		BAN::ErrorOr<Node> evaluate(BAN::StringView);

		// returns empty scope if object already exited
		BAN::ErrorOr<Scope> add_named_object(const Scope& scope, const NameString& name_string, Node&& node);
		BAN::ErrorOr<Scope> add_named_object(const Scope& scope, const NameString& name_string, Reference* reference);

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

	private:
		BAN::ErrorOr<Scope> resolve_path(const Scope& scope, const NameString& name_string);

		BAN::ErrorOr<void> initialize_scope(const Scope& scope);

		BAN::ErrorOr<void> opregion_call_reg(const Scope& scope, const Node& opregion);

	private:
		bool m_has_parsed_namespace { false };
		BAN::HashMap<Scope, Reference*> m_named_objects;
		BAN::HashMap<Scope, uint32_t> m_called_reg_bitmaps;
	};

}
