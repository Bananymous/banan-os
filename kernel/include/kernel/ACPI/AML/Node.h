#pragma once

#include <BAN/Array.h>
#include <BAN/ByteSpan.h>
#include <BAN/LinkedList.h>
#include <BAN/NoCopyMove.h>
#include <BAN/Optional.h>
#include <BAN/StringView.h>
#include <BAN/Vector.h>

#include <kernel/ACPI/AML/Scope.h>
#include <kernel/ACPI/Headers.h>
#include <kernel/Lock/Mutex.h>

#define AML_DUMP_FUNCTION_CALLS 0
#define AML_ENABLE_DEBUG 1

namespace Kernel::ACPI::AML
{

	struct NameString
	{
		BAN_NON_COPYABLE(NameString);
	public:
		NameString() = default;
		NameString(NameString&& other)
			: base(other.base)
			, parts(BAN::move(other.parts))
		{}
		NameString& operator=(NameString&& other)
		{
			base = other.base;
			parts = BAN::move(other.parts);
			return *this;
		}

		static BAN::ErrorOr<NameString> from_string(BAN::StringView name);

		BAN::ErrorOr<NameString> copy() const
		{
			NameString result;
			result.base = this->base;

			TRY(result.parts.resize(parts.size()));
			for (size_t i = 0; i < parts.size(); i++)
				result.parts[i] = parts[i];

			return result;
		}

		static constexpr uint32_t base_root = -1;
		uint32_t base { 0 };
		BAN::Vector<uint32_t> parts;
	};

	template<typename T1, typename T2>
	struct Pair
	{
		T1 elem1;
		T2 elem2;
	};

	struct Node;
	struct ParseContext;
	struct Reference;

	struct Mutex
	{
		Kernel::Mutex mutex;
		uint8_t sync_level;
		bool global_lock;
		uint32_t ref_count;
	};

	struct Buffer
	{
		BAN::StringView as_sv() const
		{
			return BAN::StringView(reinterpret_cast<const char*>(bytes), size);
		}

		uint64_t size;
		uint32_t ref_count;
		uint8_t bytes[];
	};

	struct OpRegion
	{
		GAS::AddressSpaceID address_space;
		uint64_t offset;
		uint64_t length;
	};

	struct FieldUnit
	{
		enum class Type {
			Field,
			IndexField,
			BankField,
		};
		uint64_t offset;
		uint64_t length;
		uint8_t flags;
		Type type;

		union {
			struct {
				OpRegion opregion;
			} field;
			struct {
				Reference* index;
				Reference* data;
			} index_field;
			struct {
				OpRegion opregion;
				Reference* bank_selector;
				uint64_t bank_value;
			} bank_field;
		} as;
	};

	struct Package
	{
		struct Element
		{
			struct Location {
				NameString name;
				Scope scope;
			};

			bool resolved { true };
			union {
				Node* node { nullptr };
				Location* location;
			} value;
		};

		uint64_t num_elements;
		uint32_t ref_count;
		Element elements[];
	};

	struct Node
	{
		BAN_NON_COPYABLE(Node);
	public:
		Node() = default;
		~Node() { clear(); }

		Node(Node&& other) { *this = BAN::move(other); }
		Node& operator=(Node&&);

		enum class Type
		{
			Uninitialized,
			Debug,
			Integer,
			String,
			Buffer,
			Package,
			BufferField,
			OpRegion,
			FieldUnit,
			Event,
			Device,
			Processor,
			PowerResource,
			ThermalZone,
			Method,
			Mutex,
			// FIXME: Index should not be its own type
			//        parsing index should return references
			Index,
			Reference,
			PredefinedScope,
			Count
		} type { Type::Uninitialized };

		inline bool is_scope() const
		{
			switch (type)
			{
				case Type::Device:
				case Type::Processor:
				case Type::PowerResource:
				case Type::ThermalZone:
				case Type::PredefinedScope:
					return true;
				default:
					return false;
			}
			ASSERT_NOT_REACHED();
		}

		union
		{
			struct {
				uint64_t value;
			} integer;
			Package* package;
			Buffer* str_buf;
			struct {
				Buffer* buffer;
				uint64_t bit_offset;
				uint64_t bit_count;
			} buffer_field;
			OpRegion opregion;
			FieldUnit field_unit;
			struct {
				uint32_t signal_count;
			} event;
			struct {
				uint8_t storage[sizeof(Kernel::Mutex)];
				const uint8_t* start;
				size_t length;
				uint8_t arg_count;
				BAN::ErrorOr<Node> (*override_func)(const BAN::Array<Reference*, 7>&);
				bool serialized;
				Mutex* mutex;
			} method;
			Mutex* mutex;
			struct {
				Node::Type type;
				union {
					Buffer* str_buf;
					Package* package;
				} as;
				uint64_t index;
			} index;
			Reference* reference;
		} as;

		BAN::ErrorOr<Node> copy() const;

		void clear();
	};

	struct Reference
	{
		Node node {};
		uint32_t ref_count { 1 };
	};

	struct ParseContext
	{
		Scope scope;
		BAN::ConstByteSpan aml_data;

		uint32_t call_depth { 0 };
		BAN::Array<Reference*, 8> locals;
		BAN::Array<Reference*, 7> args;

		BAN::LinkedList<Scope> created_nodes;

		~ParseContext();
		BAN::ErrorOr<void> allocate_locals();
	};

	enum class ExecutionFlow
	{
		Normal,
		Break,
		Continue,
		Return,
	};
	using ExecutionFlowResult = Pair<ExecutionFlow, BAN::Optional<Node>>;

	enum Conversion : uint8_t
	{
		ConvInteger = 1,
		ConvBuffer  = 2,
		ConvString  = 4,
	};

	BAN::ErrorOr<Node> parse_node(ParseContext& context, bool return_ref = false);
	BAN::ErrorOr<ExecutionFlowResult> parse_node_or_execution_flow(ParseContext& context);

	BAN::ErrorOr<NameString> parse_name_string(BAN::ConstByteSpan& aml_data);
	BAN::ErrorOr<BAN::ConstByteSpan> parse_pkg(BAN::ConstByteSpan& aml_data);

	BAN::ErrorOr<Node> convert_node(Node&& source, uint8_t conversion, uint64_t max_length);
	BAN::ErrorOr<Node> convert_node(Node&& source, const Node& target);

	BAN::ErrorOr<Node> evaluate_node(const Scope& node_path, const Node& node);

	// If method has no return, it will return <integer 0>
	BAN::ErrorOr<Node> method_call(const Scope& scope, const Node& method, BAN::Array<Reference*, 7>&& args, uint32_t call_depth = 0);

}

namespace BAN::Formatter
{

	template<typename F>
	void print_argument(F putc, const Kernel::ACPI::AML::NameString& name_string, const ValueFormat&)
	{
		if (name_string.base == Kernel::ACPI::AML::NameString::base_root)
			putc('\\');
		else for (uint32_t i = 0; i < name_string.base; i++)
			putc('^');
		for (size_t i = 0; i < name_string.parts.size(); i++) {
			if (i != 0)
				putc('.');
			const char* name_seg = reinterpret_cast<const char*>(&name_string.parts[i]);
			putc(name_seg[0]); putc(name_seg[1]); putc(name_seg[2]); putc(name_seg[3]);
		}
	}

	template<typename F>
	void print_argument(F putc, const Kernel::ACPI::AML::Buffer& buffer, const ValueFormat&)
	{
		static constexpr size_t max_elements { 16 };

		print(putc, "<buffer '");
		if (buffer.size)
			print(putc, "{2H}", buffer.bytes[0]);
		for (size_t i = 1; i < buffer.size && i < max_elements; i++)
			print(putc, " {2H}", buffer.bytes[i]);
		if (buffer.size > max_elements)
			print(putc, "...");
		print(putc, "'>");
	}

	template<typename F>
	void print_argument(F putc, const Kernel::ACPI::AML::Package& package, const ValueFormat&)
	{
		print(putc, "<package '{} elements'>", package.num_elements);
	}

	template<typename F>
	void print_argument(F putc, const Kernel::ACPI::AML::Node& node, const ValueFormat&)
	{
		switch (node.type)
		{
			case Kernel::ACPI::AML::Node::Type::Uninitialized:
				print(putc, "<uninitialized>");
				break;
			case Kernel::ACPI::AML::Node::Type::Debug:
				print(putc, "<debug>");
				break;
			case Kernel::ACPI::AML::Node::Type::Integer:
				print(putc, "<integer 0x{H}>", node.as.integer.value);
				break;
			case Kernel::ACPI::AML::Node::Type::String:
				print(putc, "<string '{}'>", node.as.str_buf->as_sv());
				break;
			case Kernel::ACPI::AML::Node::Type::Package:
				print(putc, "{}", *node.as.package);
				break;
			case Kernel::ACPI::AML::Node::Type::Buffer:
				print(putc, "{}", *node.as.str_buf);
				break;
			case Kernel::ACPI::AML::Node::Type::BufferField:
				print(putc, "<buffer field '{} bytes, offset 0x{H}, bit count {}'>",
					node.as.buffer_field.buffer->size,
					node.as.buffer_field.bit_offset,
					node.as.buffer_field.bit_count
				);
				break;
			case Kernel::ACPI::AML::Node::Type::OpRegion:
				print(putc, "<opregion 'type {2H}, offset 0x{H}, length 0x{H}'>",
					static_cast<uint8_t>(node.as.opregion.address_space),
					node.as.opregion.offset,
					node.as.opregion.length
				);
				break;
			case Kernel::ACPI::AML::Node::Type::FieldUnit:
				print(putc, "<field unit ({}), 'offset 0x{H}, length 0x{H}'>",
					static_cast<uint8_t>(node.as.field_unit.type),
					node.as.field_unit.offset,
					node.as.field_unit.length
				);
				break;
			case Kernel::ACPI::AML::Node::Type::Event:
				print(putc, "<event '{} signals'>", node.as.event.signal_count);
				break;
			case Kernel::ACPI::AML::Node::Type::Device:
				print(putc, "<device>");
				break;
			case Kernel::ACPI::AML::Node::Type::Processor:
				print(putc, "<processor>");
				break;
			case Kernel::ACPI::AML::Node::Type::PowerResource:
				print(putc, "<power resouce>");
				break;
			case Kernel::ACPI::AML::Node::Type::ThermalZone:
				print(putc, "<thermal zone>");
				break;
			case Kernel::ACPI::AML::Node::Type::Method:
				print(putc, "<method '{} bytes'>", node.as.method.length);
				break;
			case Kernel::ACPI::AML::Node::Type::Mutex:
				print(putc, "<mutex>");
				break;
			case Kernel::ACPI::AML::Node::Type::Index:
				switch (node.as.index.type)
				{
					case Kernel::ACPI::AML::Node::Type::String:
					case Kernel::ACPI::AML::Node::Type::Buffer:
						print(putc, "<index {}, {}>", *node.as.index.as.str_buf, node.as.index.index);
						break;
					case Kernel::ACPI::AML::Node::Type::Package:
						print(putc, "<index {}, {}>", *node.as.index.as.package, node.as.index.index);
						break;
					default:
						print(putc, "<index {}??, {}>", (uint32_t)node.as.index.type, node.as.index.index);
						break;
				}
				break;
			case Kernel::ACPI::AML::Node::Type::Reference:
				print(putc, "<reference {}, {} refs>", node.as.reference->node, node.as.reference->ref_count);
				break;
			case Kernel::ACPI::AML::Node::Type::PredefinedScope:
				print(putc, "<scope>");
				break;
			case Kernel::ACPI::AML::Node::Type::Count:
				ASSERT_NOT_REACHED();
		}
	}

}
