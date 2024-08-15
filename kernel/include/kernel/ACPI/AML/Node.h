#pragma once

#include <BAN/ByteSpan.h>
#include <BAN/Optional.h>
#include <BAN/RefPtr.h>
#include <BAN/Vector.h>
#include <kernel/ACPI/AML/Utils.h>

namespace Kernel::ACPI::AML
{

	struct Buffer;
	struct Integer;
	struct String;

	struct Node : public BAN::RefCounted<Node>
	{
		static uint64_t total_node_count;

		enum Conversion : uint8_t
		{
			ConvBuffer      = 1 << 0,
			ConvBufferField = 1 << 1,
			ConvFieldUnit   = 1 << 2,
			ConvInteger     = 1 << 3,
			ConvString      = 1 << 4,
		};

		enum class Type : uint8_t
		{
			None,
			Alias,
			BankFieldElement,
			Buffer,
			BufferField,
			Debug,
			Device,
			Event,
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

		[[nodiscard]] virtual BAN::RefPtr<AML::Node> convert(uint8_t mask) = 0;
		[[nodiscard]] virtual BAN::RefPtr<Node> copy() { return this; }
		[[nodiscard]] virtual BAN::RefPtr<AML::Node> store(BAN::RefPtr<AML::Node>) { AML_TODO("store, type {}", static_cast<uint8_t>(type)); return {}; }

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
			Breaked,
			Continued,
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
		bool breaked() const { return m_result == Result::Breaked; }
		bool continued() const { return m_result == Result::Continued; }

		BAN::RefPtr<Node> node()
		{
			return m_node;
		}

	private:
		Result m_result = Result::Failure;
		BAN::RefPtr<Node> m_node;
	};
	ParseResult parse_object(ParseContext& context);

}
