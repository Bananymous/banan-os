#pragma once

#include <BAN/ByteSpan.h>
#include <BAN/Optional.h>
#include <BAN/RefPtr.h>
#include <BAN/Vector.h>
#include <kernel/ACPI/AML/Utils.h>

namespace Kernel::ACPI::AML
{

	struct Node : public BAN::RefCounted<Node>
	{
		static uint64_t total_node_count;

		enum class Type
		{
			BankFieldElement,
			Buffer,
			BufferField,
			Device,
			FieldElement,
			IndexFieldElement,
			Integer,
			Method,
			Mutex,
			Name,
			Namespace,
			OpRegion,
			Package,
			PackageElement,
			PowerResource,
			Processor,
			Reference,
			Register,
			String,
			ThermalZone,
		};
		const Type type;

		Node(Type type) : type(type) { total_node_count++; }
		virtual ~Node() { total_node_count--; }

		virtual bool is_scope() const { return false; }

		[[nodiscard]] BAN::Optional<uint64_t> as_integer();
		[[nodiscard]] virtual BAN::RefPtr<AML::Node> evaluate() { AML_TODO("evaluate, type {}", static_cast<uint8_t>(type)); return nullptr; }
		[[nodiscard]] virtual bool store(BAN::RefPtr<AML::Node>) { AML_TODO("store, type {}", static_cast<uint8_t>(type)); return false; }

		virtual void debug_print(int indent) const = 0;
	};

	struct ParseContext;
	struct ParseResult
	{
		static ParseResult Failure;
		static ParseResult Success;

		enum class Result
		{
			Success,
			Failure,
			Returned,
		};

		ParseResult(Result success)
			: m_result(success)
		{}
		ParseResult(Result success, BAN::RefPtr<Node> node)
			: m_result(success)
			, m_node(BAN::move(node))
		{}
		ParseResult(BAN::RefPtr<Node> node)
			: m_result(Result::Success)
			, m_node(BAN::move(node))
		{
			ASSERT(m_node);
		}

		bool success() const { return m_result == Result::Success; }
		bool returned() const { return m_result == Result::Returned; }

		BAN::RefPtr<Node> node()
		{
			ASSERT(m_node);
			return m_node;
		}

	private:
		Result m_result = Result::Failure;
		BAN::RefPtr<Node> m_node;
	};
	ParseResult parse_object(ParseContext& context);

}
