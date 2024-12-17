#include <BAN/Assert.h>
#include <BAN/String.h>

#include <kernel/ACPI/ACPI.h>
#include <kernel/ACPI/AML/Bytes.h>
#include <kernel/ACPI/AML/Namespace.h>
#include <kernel/ACPI/AML/Node.h>
#include <kernel/ACPI/AML/OpRegion.h>
#include <kernel/Processor.h>
#include <kernel/Timer/Timer.h>

#include <ctype.h>

namespace Kernel::ACPI::AML
{

	static constexpr uint64_t ONES = BAN::numeric_limits<uint64_t>::max();
	static constexpr uint64_t ZERO = BAN::numeric_limits<uint64_t>::min();

	BAN::ErrorOr<NameString> parse_name_string(BAN::ConstByteSpan& aml_data)
	{
		dprintln_if(AML_DUMP_FUNCTION_CALLS, "parse_name_string");

		if (aml_data.empty())
			return BAN::Error::from_errno(ENODATA);

		NameString name {};

		switch (aml_data[0])
		{
			case '^':
				while (!aml_data.empty() && aml_data[0] == '^') {
					name.base++;
					aml_data = aml_data.slice(1);
				}
				break;
			case '\\':
				name.base = NameString::base_root;
				aml_data = aml_data.slice(1);
				break;
		}

		if (aml_data.empty())
			return BAN::Error::from_errno(ENODATA);

		size_t name_seg_count = 1;
		switch (aml_data[0])
		{
			case 0:
				name_seg_count = 0;
				aml_data = aml_data.slice(1);
				break;
			case '.':
				name_seg_count = 2;
				aml_data = aml_data.slice(1);
				break;
			case '/':
				if (aml_data.size() < 2)
					return BAN::Error::from_errno(ENODATA);
				name_seg_count = aml_data[1];
				aml_data = aml_data.slice(2);
				break;
		}

		if (aml_data.size() < name_seg_count * 4)
			return BAN::Error::from_errno(ENODATA);

		TRY(name.parts.resize(name_seg_count));
		for (size_t i = 0; i < name_seg_count; i++)
		{
			if (!is_lead_name_char(aml_data[0]) || !is_name_char(aml_data[1]) || !is_name_char(aml_data[2]) || !is_name_char(aml_data[3]))
			{
				dwarnln("Invalid NameSeg {2H}, {2H}, {2H}, {2H}",
					aml_data[0], aml_data[1], aml_data[2], aml_data[3]
				);
				return BAN::Error::from_errno(EINVAL);
			}
			name.parts[i] = aml_data.as<const uint32_t>();
			aml_data = aml_data.slice(4);
		}

		return name;
	}

	static BAN::ErrorOr<Node> parse_integer(BAN::ConstByteSpan& aml_data)
	{
		dprintln_if(AML_DUMP_FUNCTION_CALLS, "parse_integer");

		ASSERT(!aml_data.empty());

		Node result {};
		result.type = Node::Type::Integer;

		size_t byte_count = 0;
		switch (static_cast<AML::Byte>(aml_data[0]))
		{
			case AML::Byte::ZeroOp:
				result.as.integer.value = ZERO;
				break;
			case AML::Byte::OneOp:
				result.as.integer.value = 1;
				break;
			case AML::Byte::OnesOp:
				result.as.integer.value = ONES;
				break;
			case AML::Byte::BytePrefix:
				byte_count = 1;
				break;
			case AML::Byte::WordPrefix:
				byte_count = 2;
				break;
			case AML::Byte::DWordPrefix:
				byte_count = 4;
				break;
			case AML::Byte::QWordPrefix:
				byte_count = 8;
				break;
			default:
				ASSERT_NOT_REACHED();
		}

		aml_data = aml_data.slice(1);

		if (byte_count)
		{
			if (aml_data.size() < byte_count)
				return BAN::Error::from_errno(ENODATA);
			result.as.integer.value = 0;
			for (size_t i = 0; i < byte_count; i++)
				result.as.integer.value |= static_cast<uint64_t>(aml_data[i]) << (i * 8);
			aml_data = aml_data.slice(byte_count);
		}

		return result;
	}

	static BAN::ErrorOr<Node> parse_string(BAN::ConstByteSpan& aml_data)
	{
		dprintln_if(AML_DUMP_FUNCTION_CALLS, "parse_string");

		ASSERT(!aml_data.empty());
		ASSERT(static_cast<AML::Byte>(aml_data[0]) == AML::Byte::StringPrefix);
		aml_data = aml_data.slice(1);

		size_t len = 0;
		for (; len < aml_data.size() && aml_data[len]; len++)
		{
			if (!(0x01 <= aml_data[len] && aml_data[len] <= 0x7F))
			{
				dwarnln("Invalid byte {2H} in a string", aml_data[len]);
				return BAN::Error::from_errno(EINVAL);
			}
		}
		if (len >= aml_data.size())
			return BAN::Error::from_errno(ENODATA);

		Node result {};
		result.type = Node::Type::String;
		result.as.str_buf = static_cast<Buffer*>(kmalloc(sizeof(Buffer) + len));
		result.as.str_buf->size = len;
		result.as.str_buf->ref_count = 1;
		if (result.as.str_buf == nullptr)
			return BAN::Error::from_errno(ENOMEM);
		memcpy(result.as.str_buf->bytes, aml_data.as_span<const uint8_t>().data(), len);

		aml_data = aml_data.slice(len + 1);

		return result;
	}

	BAN::ErrorOr<BAN::ConstByteSpan> parse_pkg(BAN::ConstByteSpan& aml_data)
	{
		dprintln_if(AML_DUMP_FUNCTION_CALLS, "parse_pkg");

		if (aml_data.empty())
			return BAN::Error::from_errno(ENODATA);

		const uint32_t encoding_length = (aml_data[0] >> 6) + 1;
		if (aml_data.size() < encoding_length)
			return BAN::Error::from_errno(ENODATA);

		uint32_t pkg_length = 0;
		switch (encoding_length)
		{
			case 1:
				pkg_length |= aml_data[0] & 0x3F;
				break;
			case 2:
				pkg_length |= aml_data[0] & 0x0F;
				pkg_length |= aml_data[1] << 4;
				break;
			case 3:
				pkg_length |= aml_data[0] & 0x0F;
				pkg_length |= aml_data[1] << 4;
				pkg_length |= aml_data[2] << 12;
				break;
			case 4:
				pkg_length |= aml_data[0] & 0x0F;
				pkg_length |= aml_data[1] << 4;
				pkg_length |= aml_data[2] << 12;
				pkg_length |= aml_data[3] << 20;
				break;
		}

		if (aml_data.size() < pkg_length)
			return BAN::Error::from_errno(ENODATA);

		auto result = aml_data.slice(0, pkg_length).slice(encoding_length);
		aml_data = aml_data.slice(pkg_length);
		return result;
	}

	static BAN::ErrorOr<void> resolve_package_element(Package::Element& element, bool error_if_not_exists)
	{
		if (element.resolved)
		{
			if (element.value.node)
				return {};
			element.value.node = new Node();
			if (element.value.node == nullptr)
				return BAN::Error::from_errno(ENOMEM);
			element.value.node->type = Node::Type::Uninitialized;
			return {};
		}

		ASSERT(element.value.location);
		auto [_, resolved_obj] = TRY(Namespace::root_namespace().find_named_object(element.value.location->scope, element.value.location->name));
		if (resolved_obj == nullptr)
		{
			if (!error_if_not_exists)
				return {};
			dwarnln("Could not resolve '{}'.'{}'");
			return BAN::Error::from_errno(ENOENT);
		}

		Node* new_node = new Node();
		if (new_node == nullptr)
			return BAN::Error::from_errno(ENOMEM);

		switch (resolved_obj->node.type)
		{
			case Node::Type::Device:
			case Node::Type::Event:
			case Node::Type::Method:
			case Node::Type::Mutex:
			case Node::Type::OpRegion:
			case Node::Type::PowerResource:
			case Node::Type::Processor:
			case Node::Type::ThermalZone:
			case Node::Type::PredefinedScope:
				new_node->type = Node::Type::Reference;
				new_node->as.reference = resolved_obj;
				new_node->as.reference->ref_count++;
				break;
			default:
				*new_node = TRY(resolved_obj->node.copy());
				break;
		}

		delete element.value.location;
		element.resolved = true;
		element.value.node = new_node;

		return {};
	}

	static BAN::ErrorOr<Node> parse_package_op(ParseContext& context)
	{
		dprintln_if(AML_DUMP_FUNCTION_CALLS, "parse_package_op");

		ASSERT(!context.aml_data.empty());
		const auto opcode = static_cast<AML::Byte>(context.aml_data[0]);
		context.aml_data = context.aml_data.slice(1);

		auto package_pkg = TRY(parse_pkg(context.aml_data));
		if (package_pkg.empty())
			return BAN::Error::from_errno(ENODATA);

		auto old_aml_data = context.aml_data;
		context.aml_data = package_pkg;

		uint64_t num_elements = 0;
		switch (opcode)
		{
			case AML::Byte::PackageOp:
				num_elements = context.aml_data[0];
				context.aml_data = context.aml_data.slice(1);
				break;
			case AML::Byte::VarPackageOp:
			{
				auto node = TRY(parse_node(context));
				node = TRY(convert_node(BAN::move(node), ConvInteger, sizeof(uint64_t)));
				num_elements = node.as.integer.value;
				break;
			}
			default:
				ASSERT_NOT_REACHED();
		}

		Node result {};
		result.type = Node::Type::Package;
		result.as.package = static_cast<Package*>(kmalloc(sizeof(Package) + num_elements * sizeof(Package::Element)));
		if (result.as.package == nullptr)
			return BAN::Error::from_errno(ENOMEM);
		result.as.package->num_elements = num_elements;
		result.as.package->ref_count = 1;

		size_t i = 0;
		for (; i < num_elements && !context.aml_data.empty(); i++)
		{
			auto& element = result.as.package->elements[i];

			if (is_name_string_start(context.aml_data[0]))
			{
				auto name_string = TRY(parse_name_string(context.aml_data));

				element.resolved = false;
				element.value.location = new Package::Element::Location();
				if (element.value.location == nullptr)
					return BAN::Error::from_errno(ENOMEM);
				element.value.location->name = BAN::move(name_string);
				element.value.location->scope = TRY(context.scope.copy());

				TRY(resolve_package_element(element, false));
				continue;
			}

			element.resolved = true;
			element.value.node = new Node();
			element.value.node->type = Node::Type::Uninitialized;
			if (element.value.node == nullptr)
				return BAN::Error::from_errno(ENOMEM);
			*element.value.node = TRY(parse_node(context));
		}
		for (; i < num_elements; i++)
		{
			result.as.package->elements[i].resolved = true;
			result.as.package->elements[i].value.node = nullptr;
		}
		context.aml_data = old_aml_data;

		return result;
	}

	static BAN::ErrorOr<Node> parse_buffer_op(ParseContext& context)
	{
		dprintln_if(AML_DUMP_FUNCTION_CALLS, "parse_buffer_op");

		ASSERT(!context.aml_data.empty());
		ASSERT(static_cast<AML::Byte>(context.aml_data[0]) == AML::Byte::BufferOp);
		context.aml_data = context.aml_data.slice(1);

		auto buffer_pkg = TRY(parse_pkg(context.aml_data));
		if (buffer_pkg.empty())
			return BAN::Error::from_errno(ENODATA);

		auto old_aml_data = context.aml_data;
		context.aml_data = buffer_pkg;

		auto buffer_size_node = TRY(convert_node(TRY(parse_node(context)), ConvInteger, sizeof(uint64_t)));

		const uint64_t buffer_size = BAN::Math::max<uint64_t>(buffer_size_node.as.integer.value, context.aml_data.size());

		Node result {};
		result.type = Node::Type::Buffer;
		result.as.str_buf = static_cast<Buffer*>(kmalloc(sizeof(Buffer) + buffer_size));
		if (result.as.str_buf == nullptr)
			return BAN::Error::from_errno(ENOMEM);
		result.as.str_buf->size = buffer_size;
		result.as.str_buf->ref_count = 1;

		if (context.aml_data.size() > 0)
			memcpy(result.as.str_buf->bytes, context.aml_data.data(), context.aml_data.size());
		if (context.aml_data.size() < buffer_size)
			memset(result.as.str_buf->bytes + context.aml_data.size(), 0, buffer_size - context.aml_data.size());

		context.aml_data = old_aml_data;

		return result;
	}

	static BAN::ErrorOr<Node> parse_logical_op(ParseContext& context)
	{
		dprintln_if(AML_DUMP_FUNCTION_CALLS, "parse_logical_op");

		ASSERT(!context.aml_data.empty());
		const auto opcode = static_cast<AML::Byte>(context.aml_data[0]);
		context.aml_data = context.aml_data.slice(1);

		Node dummy_integer {};
		dummy_integer.type = Node::Type::Integer;
		dummy_integer.as.integer.value = 0;

		auto lhs = TRY(parse_node(context));

		if (opcode == AML::Byte::LNotOp)
		{
			lhs = TRY(convert_node(BAN::move(lhs), ConvInteger, sizeof(uint64_t)));

			Node result {};
			result.type = Node::Type::Integer;
			result.as.integer.value = lhs.as.integer.value ? ZERO : ONES;
			return result;
		}

		auto rhs = TRY(parse_node(context));

		bool (*compare)(int) = nullptr;
		switch (opcode)
		{
			case AML::Byte::LAndOp:
			case AML::Byte::LOrOp:
			{
				lhs = TRY(convert_node(BAN::move(lhs), ConvInteger, sizeof(uint64_t)));
				rhs = TRY(convert_node(BAN::move(rhs), ConvInteger, sizeof(uint64_t)));

				Node result {};
				result.type = Node::Type::Integer;
				if (opcode == AML::Byte::LAndOp)
					result.as.integer.value = (lhs.as.integer.value && rhs.as.integer.value) ? ONES : ZERO;
				else
					result.as.integer.value = (lhs.as.integer.value || rhs.as.integer.value) ? ONES : ZERO;

				return result;
			}
			case AML::Byte::LEqualOp:
				compare = [](int val) { return val == 0; };
				break;
			case AML::Byte::LLessOp:
				compare = [](int val) { return val < 0; };
				break;
			case AML::Byte::LGreaterOp:
				compare = [](int val) { return val > 0; };
				break;
			default:
				ASSERT_NOT_REACHED();
		}

		lhs = TRY(convert_node(BAN::move(lhs), ConvInteger | ConvString | ConvBuffer, ONES));

		int (*normalize)(const Node&, const Node&) = nullptr;
		switch (lhs.type)
		{
			case Node::Type::Integer:
				normalize = [](const Node& a, const Node& b) -> int {
					if (a.as.integer.value == b.as.integer.value)
						return 0;
					return a.as.integer.value < b.as.integer.value ? -1 : 1;
				};
				break;
			case Node::Type::String:
			case Node::Type::Buffer:
				normalize = [](const Node& a, const Node& b) -> int {
					const size_t bytes = BAN::Math::min(a.as.str_buf->size, b.as.str_buf->size);
					if (int ret = memcmp(a.as.str_buf->bytes, b.as.str_buf->bytes, bytes))
						return ret;
					return a.as.str_buf->size < b.as.str_buf->size;
				};
				break;
			default:
				ASSERT_NOT_REACHED();
		}

		rhs = TRY(convert_node(BAN::move(rhs), lhs));

		Node result {};
		result.type = Node::Type::Integer;
		result.as.integer.value = compare(normalize(lhs, rhs)) ? ONES : ZERO;

		return result;
	}

	static BAN::ErrorOr<Node> parse_index_op(ParseContext& context);

	enum class TargetType { Reference, Local, Arg, Debug };

	using SuperNameResult = Pair<TargetType, Reference*>;
	static BAN::ErrorOr<SuperNameResult> parse_super_name(ParseContext& context, bool error_if_not_exists)
	{
		dprintln_if(AML_DUMP_FUNCTION_CALLS, "parse_super_name");

		auto& aml_data = context.aml_data;

		const auto opcode     = static_cast<AML::Byte>(aml_data[0]);
		const auto ext_opcode = static_cast<AML::ExtOp>(aml_data.size() >= 2 ? aml_data[1] : 0);
		if (opcode == AML::Byte::IndexOp)
		{
			// FIXME: memory leak

			Reference* reference = new Reference();
			if (reference == nullptr)
				return BAN::Error::from_errno(ENOMEM);
			reference->node = TRY(parse_index_op(context));
			reference->ref_count = 1;

			return SuperNameResult { TargetType::Reference, reference };
		}
		else if (is_name_string_start(aml_data[0]))
		{
			auto target = TRY(parse_name_string(aml_data));
			auto [_, named_object] = TRY(Namespace::root_namespace().find_named_object(context.scope, target));
			if (error_if_not_exists && named_object == nullptr)
			{
				dwarnln("SuperName '{}'.'{}' not found", context.scope, target);
				return BAN::Error::from_errno(ENOENT);
			}
			return SuperNameResult { TargetType::Reference, named_object };
		}
		else if (opcode == AML::Byte::ExtOpPrefix && ext_opcode == AML::ExtOp::DebugOp)
		{
			aml_data = aml_data.slice(2);
			return SuperNameResult { TargetType::Debug, nullptr };
		}
		else if (AML::Byte::Local0 <= opcode && opcode <= AML::Byte::Local7)
		{
			const uint8_t local_index = aml_data[0] - static_cast<uint8_t>(AML::Byte::Local0);
			aml_data = aml_data.slice(1);
			ASSERT(context.locals[local_index]);
			return SuperNameResult { TargetType::Local, context.locals[local_index] };
		}
		else if (AML::Byte::Arg0 <= opcode && opcode <= AML::Byte::Arg6)
		{
			const uint8_t arg_index = aml_data[0] - static_cast<uint8_t>(AML::Byte::Arg0);
			aml_data = aml_data.slice(1);

			if (context.args[arg_index] == nullptr) {
				dwarnln("Trying to reference uninitialized arg");
				return BAN::Error::from_errno(EINVAL);
			}

			return SuperNameResult { TargetType::Local, context.args[arg_index] };
		}

		const bool is_ext = (opcode == AML::Byte::ExtOpPrefix) && (aml_data.size() >= 2);
		dwarnln("TODO: SuperName {2H}{}", aml_data[is_ext ? 1 : 0], is_ext ? "e" : "");
		return BAN::Error::from_errno(ENOTSUP);
	}

	template<typename T> requires (BAN::is_same_v<T, Node> || BAN::is_same_v<T, const Node>)
	static BAN::ErrorOr<T&> underlying_node(T& node)
	{
		switch (node.type)
		{
			case Node::Type::Index:
				dwarnln("TODO: Underlying node of index");
				return BAN::Error::from_errno(ENOTSUP);
			case Node::Type::Reference:
				return node.as.reference->node;
			default:
				return node;
		}
	}

	BAN::ErrorOr<Node> convert_node(Node&& source, uint8_t conversion, size_t max_length)
	{
		dprintln_if(AML_DUMP_FUNCTION_CALLS, "convert_node {} -> 0b{3b}", source, conversion);

		if ((source.type == Node::Type::Integer) && (conversion & Conversion::ConvInteger))
			return source;
		if ((source.type == Node::Type::Buffer) && (conversion & Conversion::ConvBuffer))
			return source;
		if ((source.type == Node::Type::String) && (conversion & Conversion::ConvString))
			return source;

		if (source.type == Node::Type::FieldUnit)
			return convert_from_field_unit(source, conversion, max_length);

		if (conversion & Conversion::ConvInteger)
		{
			Node result {};
			result.type = Node::Type::Integer;
			result.as.integer.value = 0;

			switch (source.type)
			{
				case Node::Type::String:
				case Node::Type::Buffer:
					memcpy(
						&result.as.integer.value,
						source.as.str_buf->bytes,
						BAN::Math::min<size_t>(source.as.str_buf->size, sizeof(uint64_t))
					);
					return result;
				case Node::Type::BufferField:
					for (size_t i = 0; i < BAN::Math::min<size_t>(source.as.buffer_field.bit_count, 64); i++)
					{
						const uint64_t idx = source.as.buffer_field.bit_offset + i;
						const uint64_t bit = (source.as.buffer_field.buffer->bytes[idx / 8] >> (idx % 8)) & 1;
						result.as.integer.value |= bit << i;
					}
					return result;
				default:
					break;
			}
		}

		if (conversion & Conversion::ConvBuffer)
		{
			Node result {};
			result.type = Node::Type::Buffer;
			result.as.str_buf = nullptr;

			switch (source.type)
			{
				case Node::Type::Integer:
					if (source.as.integer.value)
						max_length = BAN::Math::min<size_t>(max_length, BAN::Math::ilog2(source.as.integer.value) / 4 + 1);
					else
						max_length = 1;
					result.as.str_buf = static_cast<Buffer*>(kmalloc(sizeof(Buffer) + max_length));
					if (result.as.str_buf == nullptr)
						return BAN::Error::from_errno(ENOMEM);
					memcpy(result.as.str_buf->bytes, &source.as.integer.value, max_length);
					result.as.str_buf->size = max_length;
					result.as.str_buf->ref_count = 1;
					return result;
				case Node::Type::String:
				{
					max_length = BAN::Math::min<size_t>(max_length, source.as.str_buf->size + 1);

					result.as.str_buf = static_cast<Buffer*>(kmalloc(sizeof(Buffer) + max_length));
					if (result.as.str_buf == nullptr)
						return BAN::Error::from_errno(ENOMEM);
					if (max_length <= source.as.str_buf->size)
						memcpy(result.as.str_buf->bytes, source.as.str_buf->bytes, max_length);
					else
					{
						memcpy(result.as.str_buf->bytes, source.as.str_buf->bytes, max_length - 1);
						result.as.str_buf->bytes[max_length - 1] = 0x00;
					}
					result.as.str_buf->size = max_length;
					result.as.str_buf->ref_count = 1;
					return result;
				}
				case Node::Type::BufferField:
					dwarnln("TODO: buffer field to buffer");
					return BAN::Error::from_errno(ENOTSUP);
				default:
					break;
			}
		}

		if (conversion & Conversion::ConvString)
		{
			Node result {};
			result.type = Node::Type::String;
			result.as.str_buf = nullptr;

			switch (source.type)
			{
				case Node::Type::Integer:
				{
					auto string = TRY(BAN::String::formatted("{}", source.as.integer.value));
					max_length = BAN::Math::min<size_t>(max_length, string.size());
					result.as.str_buf = static_cast<Buffer*>(kmalloc(sizeof(Buffer) + max_length));
					if (result.as.str_buf == nullptr)
						return BAN::Error::from_errno(ENOMEM);
					memcpy(result.as.str_buf->bytes, string.data(), max_length);
					result.as.str_buf->size = max_length;
					result.as.str_buf->ref_count = 1;
					return result;
				}
				case Node::Type::Buffer:
					result = BAN::move(source);
					result.type = Node::Type::String;
					return result;
				case Node::Type::BufferField:
					dwarnln("TODO: buffer field to string");
					return BAN::Error::from_errno(ENOTSUP);
				default:
					break;
			}
		}

		dwarnln("Invalid conversion from {} to 0b{3b}", source, conversion);
		return BAN::Error::from_errno(EINVAL);
	}

	BAN::ErrorOr<Node> convert_node(Node&& source, const Node& target)
	{
		if (target.type == Node::Type::Uninitialized)
			return source;
		if (target.type == Node::Type::Integer)
			return convert_node(BAN::move(source), ConvInteger, 8);
		if (target.type == Node::Type::String)
			return convert_node(BAN::move(source), ConvString,  target.as.str_buf->size);
		if (target.type == Node::Type::Buffer)
			return convert_node(BAN::move(source), ConvBuffer,  target.as.str_buf->size);
		dwarnln("Invalid conversion from {} to {}", source, target);
		return BAN::Error::from_errno(EINVAL);
	}

	static BAN::ErrorOr<void> store_to_buffer_field(const Node& source, Node& target)
	{
		ASSERT(target.type == Node::Type::BufferField);

		const uint64_t bit_count = target.as.buffer_field.bit_count;
		const uint64_t bit_offset = target.as.buffer_field.bit_offset;

		const uint8_t* src_buf = nullptr;
		uint64_t src_len = 0;

		switch (source.type)
		{
			case Node::Type::String:
			case Node::Type::Buffer:
				src_buf = source.as.str_buf->bytes;
				src_len = source.as.str_buf->size;
				break;
			case Node::Type::Integer:
				src_buf = reinterpret_cast<const uint8_t*>(&source.as.integer.value);
				src_len = sizeof(uint64_t);
				break;
			default:
			{
				Node dummy = TRY(convert_node(TRY(source.copy()), ConvInteger | ConvBuffer, sizeof(uint64_t)));
				return store_to_buffer_field(dummy, target);
			}
		}

		uint64_t i = 0;
		while (i < bit_count)
		{
			const uint64_t j = bit_offset + i;

			const uint8_t i_mod = i % 8;
			const uint8_t j_mod = j % 8;

			const uint8_t max_src_bits = BAN::Math::min<uint64_t>(8 - i_mod, bit_count - i);
			const uint8_t max_dst_bits = BAN::Math::min<uint64_t>(8 - j_mod, bit_count - i);
			const uint8_t bits = BAN::Math::min(max_src_bits, max_dst_bits);

			const uint8_t mask = (1 << bits) - 1;

			const uint8_t src_byte = (i / 8 < src_len) ? src_buf[i / 8] : 0x00;

			uint8_t& dst_byte = target.as.buffer_field.buffer->bytes[j / 8];
			dst_byte &= ~(mask << j_mod);
			dst_byte |= ((src_byte >> i_mod) & mask) << j_mod;

			i += bits;
		}

		return {};
	}

	static BAN::ErrorOr<void> perform_store(const Node& source, Reference* target, TargetType target_type)
	{
		dprintln_if(AML_DUMP_FUNCTION_CALLS, "perform_store");

		if (target_type == TargetType::Debug)
		{
			dprintln_if(AML_ENABLE_DEBUG, "DEBUG: {}", source);
			return {};
		}

		ASSERT(target);

		if (target->node.type == Node::Type::BufferField)
			return store_to_buffer_field(source, target->node);

		if (target->node.type == Node::Type::FieldUnit)
			return store_to_field_unit(source, target->node);

		auto source_copy = TRY(source.copy());

		Node* index_node = nullptr;
		if (target->node.type == Node::Type::Reference)
		{
			auto& ref_target = target->node.as.reference->node;

			if (ref_target.type == Node::Type::Index)
				index_node = &ref_target;
			else
			{
				ASSERT(ref_target.type != Node::Type::Reference);
				ref_target = TRY(convert_node(BAN::move(source_copy), ref_target));
				return {};
			}
		}

		if (target->node.type == Node::Type::Index)
			index_node = &target->node;

		if (index_node)
		{
			auto& index = index_node->as.index;
			switch (index.type)
			{
				case Node::Type::String:
				case Node::Type::Buffer:
					ASSERT(index.index < index.as.str_buf->size);
					index.as.str_buf->bytes[index.index] = TRY(convert_node(BAN::move(source_copy), ConvInteger, sizeof(uint64_t))).as.integer.value;
					break;
				case Node::Type::Package:
				{
					ASSERT(index.index < index.as.package->num_elements);

					auto& pkg_element = index.as.package->elements[index.index];
					TRY(resolve_package_element(pkg_element, true));
					ASSERT(pkg_element.value.node);

					if (pkg_element.value.node->type == Node::Type::Reference)
						*pkg_element.value.node = TRY(convert_node(BAN::move(source_copy), pkg_element.value.node->as.reference->node));
					else
						*pkg_element.value.node = BAN::move(source_copy);
					break;
				}
				default:
					ASSERT_NOT_REACHED();
			}

			return {};
		}

		if (target_type == TargetType::Reference)
			target->node = TRY(convert_node(BAN::move(source_copy), target->node));
		else
			target->node = BAN::move(source_copy);

		return {};
	}

	static BAN::ErrorOr<void> store_into_target(ParseContext& context, const Node& node)
	{
		dprintln_if(AML_DUMP_FUNCTION_CALLS, "store_into_target");

		auto& aml_data = context.aml_data;

		if (aml_data.empty())
			return BAN::Error::from_errno(ENODATA);

		if (aml_data[0] == 0)
		{
			aml_data = aml_data.slice(1);
			return {};
		}

		auto [target_type, target] = TRY(parse_super_name(context, true));
		return perform_store(node, target, target_type);
	}

	static BAN::ErrorOr<Node> parse_store_op(ParseContext& context)
	{
		dprintln_if(AML_DUMP_FUNCTION_CALLS, "parse_store_op");

		ASSERT(!context.aml_data.empty());
		ASSERT(static_cast<AML::Byte>(context.aml_data[0]) == AML::Byte::StoreOp);
		context.aml_data = context.aml_data.slice(1);

		auto source = TRY(parse_node(context));

		if (context.aml_data.empty())
			return BAN::Error::from_errno(ENODATA);
		if (context.aml_data[0] == 0)
		{
			dwarnln("StoreOp in to null target");
			return BAN::Error::from_errno(EINVAL);
		}

		TRY(store_into_target(context, source));

		return source;
	}

	static BAN::ErrorOr<Node> parse_copy_object_op(ParseContext& context)
	{
		dprintln_if(AML_DUMP_FUNCTION_CALLS, "parse_copy_object_op");

		ASSERT(!context.aml_data.empty());
		ASSERT(static_cast<AML::Byte>(context.aml_data[0]) == AML::Byte::CopyObjectOp);
		context.aml_data = context.aml_data.slice(1);

		auto source = TRY(parse_node(context));

		if (context.aml_data.empty())
			return BAN::Error::from_errno(ENODATA);

		auto [target_type, target] = TRY(parse_super_name(context, true));
		switch (target_type)
		{
			case TargetType::Arg:
			case TargetType::Local:
			case TargetType::Reference:
				break;
			case TargetType::Debug:
				dwarnln("CopyObjectOp target is Debug");
				return BAN::Error::from_errno(EINVAL);
		}

		target->node = TRY(source.copy());

		return source;
	}

	static BAN::ErrorOr<Node> parse_index_op(ParseContext& context)
	{
		dprintln_if(AML_DUMP_FUNCTION_CALLS, "parse_index_op");

		ASSERT(!context.aml_data.empty());
		ASSERT(static_cast<AML::Byte>(context.aml_data[0]) == AML::Byte::IndexOp);
		context.aml_data = context.aml_data.slice(1);

		auto source_dummy = TRY(parse_node(context, true));
		auto index        = TRY(convert_node(TRY(parse_node(context)), ConvInteger, sizeof(uint64_t)));

		auto& source = TRY_REF(underlying_node(source_dummy));

		Node result {};

		switch (source.type)
		{
			case Node::Type::String:
			case Node::Type::Buffer:
				if (index.as.integer.value >= source.as.str_buf->size)
				{
					dwarnln("Invalid index {} to buffer of size {}", index.as.integer.value, source.as.str_buf->size);
					return BAN::Error::from_errno(EINVAL);
				}
				result.as.index.as.str_buf = source.as.str_buf;
				result.as.index.as.str_buf->ref_count++;
				break;
			case Node::Type::Package:
				if (index.as.integer.value >= source.as.package->num_elements)
				{
					dwarnln("Invalid index {} to package of size {}", index.as.integer.value, source.as.package->num_elements);
					return BAN::Error::from_errno(EINVAL);
				}
				result.as.index.as.package = source.as.package;
				result.as.index.as.package->ref_count++;
				break;
			default:
				dwarnln("Invalid IndexOp({}, {})", source, index);
				return BAN::Error::from_errno(EINVAL);
		}

		result.type = Node::Type::Index;
		result.as.index.type = source.type;
		result.as.index.index = index.as.integer.value;

		TRY(store_into_target(context, result));

		return result;
	}

	static BAN::ErrorOr<Node> parse_object_type_op(ParseContext& context)
	{
		dprintln_if(AML_DUMP_FUNCTION_CALLS, "parse_object_type_op");

		ASSERT(!context.aml_data.empty());
		ASSERT(static_cast<AML::Byte>(context.aml_data[0]) == AML::Byte::ObjectTypeOp);
		context.aml_data = context.aml_data.slice(1);

		auto [object_type, object] = TRY(parse_super_name(context, true));

		uint64_t value = 0;
		if (object_type == TargetType::Debug)
			value = 16;
		else
		{
			ASSERT(object);

			auto* node = &object->node;
			while (node->type == Node::Type::Reference)
				if (node->type == Node::Type::Reference)
					node = &node->as.reference->node;

			auto node_type = node->type;
			if (node_type == Node::Type::Index)
			{
				const auto& index = node->as.index;
				switch (index.type)
				{
					case Node::Type::String:
					case Node::Type::Buffer:
						node_type = Node::Type::BufferField;
						break;
					case Node::Type::Package:
						ASSERT(index.index < index.as.package->num_elements);
						TRY(resolve_package_element(index.as.package->elements[index.index], true));
						node_type = index.as.package->elements[index.index].value.node->type;
						break;
					default:
						ASSERT_NOT_REACHED();
				}
			}

			switch (node_type)
			{
				case Node::Type::Uninitialized:
				case Node::Type::PredefinedScope: value =  0; break;
				case Node::Type::Integer:         value =  1; break;
				case Node::Type::String:          value =  2; break;
				case Node::Type::Buffer:          value =  3; break;
				case Node::Type::Package:         value =  4; break;
				case Node::Type::FieldUnit:       value =  5; break;
				case Node::Type::Device:          value =  6; break;
				case Node::Type::Event:           value =  7; break;
				case Node::Type::Method:          value =  8; break;
				case Node::Type::Mutex:           value =  9; break;
				case Node::Type::OpRegion:        value = 10; break;
				case Node::Type::PowerResource:   value = 11; break;
				case Node::Type::Processor:       value = 12; break;
				case Node::Type::ThermalZone:     value = 13; break;
				case Node::Type::BufferField:     value = 14; break;
				case Node::Type::Debug:           value = 16; break;
				case Node::Type::Index:
				case Node::Type::Reference:
				case Node::Type::Count:           ASSERT_NOT_REACHED();
			}
		}

		Node result;
		result.type = Node::Type::Integer;
		result.as.integer.value = value;
		return result;
	}

	static BAN::ErrorOr<Node> sizeof_impl(const Node& node)
	{
		Node result {};
		result.type = Node::Type::Integer;
		result.as.integer.value = 0;

		switch (node.type)
		{
			case Node::Type::String:
			case Node::Type::Buffer:
				result.as.integer.value = node.as.str_buf->size;
				break;
			case Node::Type::Package:
				result.as.integer.value = node.as.package->num_elements;
				break;
			case Node::Type::Reference:
				return sizeof_impl(node.as.reference->node);
			default:
				dwarnln("SizeofOp on {}", node);
				return BAN::Error::from_errno(EINVAL);
		}

		return result;
	}

	static BAN::ErrorOr<void> parse_createfield_op(ParseContext& context)
	{
		dprintln_if(AML_DUMP_FUNCTION_CALLS, "parse_createfield_op");

		ASSERT(!context.aml_data.empty());
		const auto opcode = static_cast<AML::Byte>(context.aml_data[0]);
		context.aml_data = context.aml_data.slice(1);

		uint64_t bit_count = 0;
		bool index_in_bits = false;

		if (opcode == AML::Byte::ExtOpPrefix)
		{
			ASSERT(!context.aml_data.empty());
			ASSERT(static_cast<AML::ExtOp>(context.aml_data[0]) == AML::ExtOp::CreateFieldOp);
			context.aml_data = context.aml_data.slice(1);

			bit_count = 0;
			index_in_bits = true;
		}
		else
		{
			switch (opcode)
			{
				case AML::Byte::CreateBitFieldOp:
					bit_count = 1;
					index_in_bits = true;
					break;
				case AML::Byte::CreateByteFieldOp:
					bit_count = 8;
					index_in_bits = false;
					break;
				case AML::Byte::CreateWordFieldOp:
					bit_count = 16;
					index_in_bits = false;
					break;
				case AML::Byte::CreateDWordFieldOp:
					bit_count = 32;
					index_in_bits = false;
					break;
				case AML::Byte::CreateQWordFieldOp:
					bit_count = 64;
					index_in_bits = false;
					break;
				default:
					ASSERT_NOT_REACHED();
			}
		}

		auto buffer_node = TRY(parse_node(context));
		if (buffer_node.type != Node::Type::Buffer)
		{
			dwarnln("CreateField buffer is {}", buffer_node);
			return BAN::Error::from_errno(EINVAL);
		}

		Node dummy_integer {};
		dummy_integer.type = Node::Type::Integer;
		dummy_integer.as.integer.value = 0;

		auto index_node = TRY(convert_node(TRY(parse_node(context)), ConvInteger, sizeof(uint64_t)));

		if (bit_count == 0)
		{
			auto bit_count_node = TRY(convert_node(TRY(parse_node(context)), ConvInteger, sizeof(uint64_t)));

			bit_count = bit_count_node.as.integer.value;
			if (bit_count == 0)
			{
				dwarnln("CreateField bit count 0");
				return BAN::Error::from_errno(EINVAL);
			}
		}

		auto field_name_string = TRY(parse_name_string(context.aml_data));

		Node buffer_field;
		buffer_field.type = Node::Type::BufferField;
		buffer_field.as.buffer_field.buffer = buffer_node.as.str_buf;
		buffer_field.as.buffer_field.buffer->ref_count++;
		buffer_field.as.buffer_field.bit_count = bit_count;
		buffer_field.as.buffer_field.bit_offset = index_in_bits ? index_node.as.integer.value : index_node.as.integer.value * 8;

		TRY(Namespace::root_namespace().add_named_object(context.scope, field_name_string, BAN::move(buffer_field)));

		return {};
	}

	static BAN::ErrorOr<Node> parse_sizeof_op(ParseContext& context)
	{
		dprintln_if(AML_DUMP_FUNCTION_CALLS, "parse_sizeof_op");

		ASSERT(!context.aml_data.empty());
		ASSERT(static_cast<AML::Byte>(context.aml_data[0]) == AML::Byte::SizeOfOp);
		context.aml_data = context.aml_data.slice(1);

		auto [object_type, object] = TRY(parse_super_name(context, true));
		if (object_type == TargetType::Debug)
		{
			dwarnln("SizeofOp on debug object");
			return BAN::Error::from_errno(EINVAL);
		}

		ASSERT(object);
		return TRY(sizeof_impl(object->node));
	}

	static BAN::ErrorOr<Node> derefof_impl(const Node& source)
	{
		switch (source.type)
		{
			case Node::Type::Reference:
				return TRY(source.as.reference->node.copy());
			case Node::Type::Index:
			{
				switch (source.as.index.type)
				{
					case Node::Type::String:
					case Node::Type::Buffer:
					{
						ASSERT(source.as.index.index < source.as.index.as.str_buf->size);

						Node result;
						result.type = Node::Type::Integer;
						result.as.integer.value = source.as.index.as.str_buf->bytes[source.as.index.index];
						return result;
					}
					case Node::Type::Package:
					{
						ASSERT(source.as.index.index < source.as.index.as.package->num_elements);
						TRY(resolve_package_element(source.as.index.as.package->elements[source.as.index.index], true));
						return TRY(source.as.index.as.package->elements[source.as.index.index].value.node->copy());
					}
					default: ASSERT_NOT_REACHED();
				}
			}
			default:
				dwarnln("DerefOf of non-reference {}", source);
				return BAN::Error::from_errno(EINVAL);
		}
	}

	static BAN::ErrorOr<Node> parse_inc_dec_op(ParseContext& context)
	{
		dprintln_if(AML_DUMP_FUNCTION_CALLS, "parse_inc_dec_op");

		ASSERT(!context.aml_data.empty());
		const uint8_t opcode = context.aml_data[0];
		context.aml_data = context.aml_data.slice(1);

		uint64_t (*function)(uint64_t) = nullptr;
		switch (static_cast<AML::Byte>(opcode))
		{
			case AML::Byte::IncrementOp:
				function = [](uint64_t x) { return x + 1; };
				break;
			case AML::Byte::DecrementOp:
				function = [](uint64_t x) { return x - 1; };
				break;
			default:
				ASSERT_NOT_REACHED();
		}

		auto [type, addend_ref] = TRY(parse_super_name(context, true));
		if (type == TargetType::Debug)
		{
			dprintln_if(AML_DUMP_FUNCTION_CALLS, "UnaryIntegerOp on Debug object");

			Node result {};
			result.type = Node::Type::Integer;
			result.as.integer.value = 0;
			return result;
		}

		ASSERT(addend_ref);

		Node addend_node = TRY(addend_ref->node.copy());
		if (addend_node.type == Node::Type::Reference || addend_node.type == Node::Type::Index)
			addend_node = TRY(derefof_impl(addend_node));

		auto current = TRY(convert_node(BAN::move(addend_node), ConvInteger, sizeof(uint64_t)));

		Node result {};
		result.type = Node::Type::Integer;
		result.as.integer.value = function(current.as.integer.value);

		TRY(perform_store(result, addend_ref, type));
		return result;
	}

	static BAN::ErrorOr<Node> parse_unary_integer_op(ParseContext& context)
	{
		dprintln_if(AML_DUMP_FUNCTION_CALLS, "parse_unary_integer_op");

		ASSERT(!context.aml_data.empty());
		const auto opcode = context.aml_data[0];
		context.aml_data = context.aml_data.slice(1);

		uint64_t (*function)(uint64_t) = nullptr;

		switch (static_cast<AML::Byte>(opcode))
		{
			case AML::Byte::NotOp:
				function = [](uint64_t a) { return ~a; };
				break;
			default:
				ASSERT_NOT_REACHED();
		}

		auto value = TRY(convert_node(TRY(parse_node(context)), ConvInteger, sizeof(uint64_t)));

		Node result {};
		result.type = Node::Type::Integer;
		result.as.integer.value = function(value.as.integer.value);

		TRY(store_into_target(context, result));

		return result;
	}

	static BAN::ErrorOr<Node> parse_binary_integer_op(ParseContext& context)
	{
		dprintln_if(AML_DUMP_FUNCTION_CALLS, "parse_binary_integer_op");

		ASSERT(!context.aml_data.empty());
		const auto opcode = static_cast<AML::Byte>(context.aml_data[0]);
		context.aml_data = context.aml_data.slice(1);

		uint64_t (*function)(uint64_t, uint64_t) = nullptr;

		switch (opcode)
		{
			case AML::Byte::AddOp:
				function = [](uint64_t a, uint64_t b) { return a + b; };
				break;
			case AML::Byte::SubtractOp:
				function = [](uint64_t a, uint64_t b) { return a - b; };
				break;
			case AML::Byte::MultiplyOp:
				function = [](uint64_t a, uint64_t b) { return a * b; };
				break;
			case AML::Byte::ShiftLeftOp:
				function = [](uint64_t a, uint64_t b) { return a << b; };
				break;
			case AML::Byte::ShiftRightOp:
				function = [](uint64_t a, uint64_t b) { return a >> b; };
				break;
			case AML::Byte::AndOp:
				function = [](uint64_t a, uint64_t b) { return a & b; };
				break;
			case AML::Byte::NandOp:
				function = [](uint64_t a, uint64_t b) { return ~(a & b); };
				break;
			case AML::Byte::OrOp:
				function = [](uint64_t a, uint64_t b) { return a | b; };
				break;
			case AML::Byte::NorOp:
				function = [](uint64_t a, uint64_t b) { return ~(a | b); };
				break;
			case AML::Byte::XorOp:
				function = [](uint64_t a, uint64_t b) { return a ^ b; };
				break;
			case AML::Byte::DivideOp:
				function = [](uint64_t a, uint64_t b) { return a / b; };
				break;
			case AML::Byte::ModOp:
				function = [](uint64_t a, uint64_t b) { return a % b; };
				break;
			default:
				ASSERT_NOT_REACHED();
		}

		auto lhs = TRY(convert_node(TRY(parse_node(context)), ConvInteger, sizeof(uint64_t)));
		auto rhs = TRY(convert_node(TRY(parse_node(context)), ConvInteger, sizeof(uint64_t)));

		if (opcode == AML::Byte::DivideOp || opcode == AML::Byte::ModOp)
		{
			if (rhs.as.integer.value == 0)
			{
				dwarnln("DivideOp or ModOp divisor is zero");
				return BAN::Error::from_errno(EINVAL);
			}
		}

		Node result {};
		result.type = Node::Type::Integer;
		result.as.integer.value = function(lhs.as.integer.value, rhs.as.integer.value);

		TRY(store_into_target(context, result));

		if (opcode == AML::Byte::DivideOp)
		{
			Node remainder {};
			remainder.type = Node::Type::Integer;
			remainder.as.integer.value = function(lhs.as.integer.value, rhs.as.integer.value);
			TRY(store_into_target(context, remainder));
		}

		return result;
	}

	static BAN::ErrorOr<Node> parse_mid_op(ParseContext& context)
	{
		dprintln_if(AML_DUMP_FUNCTION_CALLS, "parse_mid_op");

		ASSERT(!context.aml_data.empty());
		ASSERT(static_cast<AML::Byte>(context.aml_data[0]) == AML::Byte::MidOp);
		context.aml_data = context.aml_data.slice(1);

		auto source           = TRY(convert_node(TRY(parse_node(context)), ConvBuffer | ConvString, ONES));
		const uint64_t index  = TRY(convert_node(TRY(parse_node(context)), ConvInteger, sizeof(uint64_t))).as.integer.value;
		const uint64_t length = TRY(convert_node(TRY(parse_node(context)), ConvInteger, sizeof(uint64_t))).as.integer.value;

		const uint64_t source_len = source.as.str_buf->size;

		uint64_t result_size = 0;
		if (index + length <= source_len)
			result_size = length;
		else if (index < source_len)
			result_size = source_len - index;

		Node result;
		result.type = source.type;
		result.as.str_buf = static_cast<Buffer*>(kmalloc(sizeof(Buffer) + result_size));
		if (result.as.str_buf == nullptr)
			return BAN::Error::from_errno(ENOTSUP);
		memcpy(result.as.str_buf->bytes, source.as.str_buf->bytes + index, result_size);
		result.as.str_buf->size = result_size;
		result.as.str_buf->ref_count = 1;

		return result;
	}

	static BAN::ErrorOr<Node> parse_concat_op(ParseContext& context)
	{
		dprintln_if(AML_DUMP_FUNCTION_CALLS, "parse_concat_op");

		ASSERT(!context.aml_data.empty());
		ASSERT(static_cast<AML::Byte>(context.aml_data[0]) == AML::Byte::ConcatOp);
		context.aml_data = context.aml_data.slice(1);

		auto source1 = TRY(convert_node(TRY(parse_node(context)), ConvInteger | ConvString | ConvBuffer, ONES));
		auto source2 = TRY(convert_node(TRY(parse_node(context)), source1));

		Node result {};
		switch (source1.type)
		{
			case AML::Node::Type::Integer:
				dwarnln("TODO: ConcatOp with integers");
				return BAN::Error::from_errno(ENOTSUP);
			case AML::Node::Type::String:
			case AML::Node::Type::Buffer:
				result.type = source1.type;
				result.as.str_buf = static_cast<Buffer*>(kmalloc(sizeof(Buffer) + source1.as.str_buf->size + source2.as.str_buf->size));
				if (result.as.str_buf == nullptr)
					return BAN::Error::from_errno(ENOMEM);
				result.as.str_buf->size = source1.as.str_buf->size + source2.as.str_buf->size;
				result.as.str_buf->ref_count = 1;
				memcpy(result.as.str_buf->bytes,                            source1.as.str_buf->bytes, source1.as.str_buf->size);
				memcpy(result.as.str_buf->bytes + source1.as.str_buf->size, source2.as.str_buf->bytes, source2.as.str_buf->size);
				break;
			default:
				ASSERT_NOT_REACHED();
		}

		TRY(store_into_target(context, result));

		return result;
	}

	static BAN::ErrorOr<Node> parse_explicit_conversion(ParseContext& context)
	{
		dprintln_if(AML_DUMP_FUNCTION_CALLS, "parse_explicit_conversion");

		ASSERT(!context.aml_data.empty());
		const uint8_t opcode = context.aml_data[0];
		context.aml_data = context.aml_data.slice(1);

		auto source = TRY(parse_node(context));
		if (source.type == Node::Type::FieldUnit || source.type == Node::Type::BufferField)
			source = TRY(convert_node(BAN::move(source), ConvInteger | ConvBuffer, ONES));

		switch (source.type)
		{
			case Node::Type::Buffer:
			case Node::Type::String:
			case Node::Type::Integer:
				break;
			default:
				dwarnln("Explicit conversion source is {}", source);
				return BAN::Error::from_errno(EINVAL);
		}

		Node result {};

		switch (static_cast<AML::Byte>(opcode))
		{
			case AML::Byte::ToDecimalStringOp:
				if (source.type == Node::Type::String)
					result = BAN::move(source);
				else
				{
					BAN::String ban_string;

					if (source.type == Node::Type::Integer)
						ban_string = TRY(BAN::String::formatted("{}", source.as.integer.value));
					else if (source.type == Node::Type::Buffer)
						for (size_t i = 0; i < source.as.str_buf->size; i++)
							TRY(ban_string.append(TRY(BAN::String::formatted("{},", source.as.str_buf->bytes[i]))));
					else
						ASSERT_NOT_REACHED();

					if (!ban_string.empty() && ban_string.back() == ',')
						ban_string.pop_back();

					result.type = Node::Type::String;
					result.as.str_buf = static_cast<Buffer*>(kmalloc(sizeof(Buffer) + ban_string.size()));
					if (result.as.str_buf == nullptr)
						return BAN::Error::from_errno(EINVAL);
					memcpy(result.as.str_buf->bytes, ban_string.data(), ban_string.size());
					result.as.str_buf->size = ban_string.size();
					result.as.str_buf->ref_count = 1;
				}
				break;
			case AML::Byte::ToHexStringOp:
				if (source.type == Node::Type::String)
					result = BAN::move(source);
				else
				{
					BAN::String ban_string;

					if (source.type == Node::Type::Integer)
						ban_string = TRY(BAN::String::formatted("0x{H}", source.as.integer.value));
					else if (source.type == Node::Type::Buffer)
						for (size_t i = 0; i < source.as.str_buf->size; i++)
							TRY(ban_string.append(TRY(BAN::String::formatted("0x{2H},", source.as.str_buf->bytes[i]))));
					else
						ASSERT_NOT_REACHED();

					if (!ban_string.empty() && ban_string.back() == ',')
						ban_string.pop_back();

					result.type = Node::Type::String;
					result.as.str_buf = static_cast<Buffer*>(kmalloc(sizeof(Buffer) + ban_string.size()));
					if (result.as.str_buf == nullptr)
						return BAN::Error::from_errno(EINVAL);
					memcpy(result.as.str_buf->bytes, ban_string.data(), ban_string.size());
					result.as.str_buf->size = ban_string.size();
					result.as.str_buf->ref_count = 1;
				}
				break;
			case AML::Byte::ToIntegerOp:
				if (source.type != Node::Type::String)
					result = TRY(convert_node(BAN::move(source), ConvInteger, sizeof(uint64_t)));
				else
				{
					const auto get_base_val =
						[](char c, uint8_t base) -> BAN::Optional<uint8_t>
						{
							if (isdigit(c) && c - '0' < base)
								return c - '0';
							if (isalpha(c) && tolower(c) - 'a' + 10 < base)
								return tolower(c) - 'a' + 10;
							return {};
					};

					auto source_sv = source.as.str_buf->as_sv();

					while (!source_sv.empty() && !isdigit(source_sv[0]) && source_sv[0] != '-' && source_sv[0] != '+')
						source_sv = source_sv.substring(1);

					const bool is_negative = source_sv.empty() ? false : source_sv[0] == '-';
					if (!source_sv.empty() && (source_sv[0] == '-' || source_sv[0] == '+'))
						source_sv = source_sv.substring(1);

					uint8_t base = 10;
					if (!source_sv.empty() && source_sv[0] == '0')
					{
						base = 8;
						source_sv = source_sv.substring(1);
						if (!source_sv.empty() && tolower(source_sv[0]) == 'x')
						{
							base = 16;
							source_sv = source_sv.substring(1);
						}
					}

					uint64_t value = 0;
					for (size_t i = 0; i < source_sv.size(); i++)
					{
						const auto to_add = get_base_val(source_sv[i], base);
						if (!to_add.has_value())
							break;

						if (BAN::Math::will_multiplication_overflow<uint64_t>(value, base) ||
							BAN::Math::will_addition_overflow<uint64_t>(value * base, to_add.value()))
						{
							value = BAN::numeric_limits<uint64_t>::max();
							break;
						}

						value = (value * base) + to_add.value();
					}

					if (is_negative)
						value = -value;

					result.type = Node::Type::Integer;
					result.as.integer.value = value;
				}
				break;
			case AML::Byte::ToBufferOp:
				result = TRY(convert_node(BAN::move(source), ConvBuffer, ONES));
				break;
			default:
				ASSERT_NOT_REACHED();
		}

		TRY(store_into_target(context, result));

		return result;
	}

	static BAN::ErrorOr<Node> parse_to_string_op(ParseContext& context)
	{
		dprintln_if(AML_DUMP_FUNCTION_CALLS, "parse_to_string_op");

		ASSERT(!context.aml_data.empty());
		ASSERT(static_cast<AML::Byte>(context.aml_data[0]) == AML::Byte::ToStringOp);
		context.aml_data = context.aml_data.slice(1);

		auto source = TRY(convert_node(TRY(parse_node(context)), ConvBuffer, ONES));
		auto length = TRY(convert_node(TRY(parse_node(context)), ConvInteger, sizeof(uint64_t))).as.integer.value;

		if (source.as.str_buf->size < length)
			length = source.as.str_buf->size;

		for (size_t i = 0; i < length; i++)
		{
			if (source.as.str_buf->bytes[i] != 0x00)
				continue;
			length = i;
			break;
		}

		Node result;
		result.type = Node::Type::String;
		result.as.str_buf = static_cast<Buffer*>(kmalloc(sizeof(Buffer) + length));
		if (result.as.str_buf == nullptr)
			return BAN::Error::from_errno(ENOMEM);
		memcpy(result.as.str_buf->bytes, source.as.str_buf->bytes, length);
		result.as.str_buf->size = length;
		result.as.str_buf->ref_count = 1;

		TRY(store_into_target(context, source));

		return result;
	}

	static BAN::ErrorOr<void> parse_alias_op(ParseContext& context)
	{
		dprintln_if(AML_DUMP_FUNCTION_CALLS, "parse_alias_op");

		ASSERT(!context.aml_data.empty());
		ASSERT(static_cast<AML::Byte>(context.aml_data[0]) == AML::Byte::AliasOp);
		context.aml_data = context.aml_data.slice(1);

		auto source_name_string = TRY(parse_name_string(context.aml_data));
		auto object_name_string = TRY(parse_name_string(context.aml_data));

		auto [_, source_ref] = TRY(Namespace::root_namespace().find_named_object(context.scope, source_name_string));
		if (source_ref == nullptr)
		{
			dwarnln("AliasOp source does not exists");
			return {};
		}

		TRY(Namespace::root_namespace().add_named_object(context.scope, object_name_string, source_ref));

		return {};
	}

	static BAN::ErrorOr<void> parse_name_op(ParseContext& context)
	{
		dprintln_if(AML_DUMP_FUNCTION_CALLS, "parse_name_op");

		ASSERT(!context.aml_data.empty());
		ASSERT(static_cast<AML::Byte>(context.aml_data[0]) == AML::Byte::NameOp);
		context.aml_data = context.aml_data.slice(1);

		auto name_string = TRY(parse_name_string(context.aml_data));
		auto object      = TRY(parse_node(context));

		auto path = TRY(Namespace::root_namespace().add_named_object(context.scope, name_string, BAN::move(object)));
		if (!path.parts.empty())
			TRY(context.created_nodes.push_back(BAN::move(path)));

		return {};
	}

	static BAN::ErrorOr<Node> parse_refof_op(ParseContext& context)
	{
		dprintln_if(AML_DUMP_FUNCTION_CALLS, "parse_refof_op");

		ASSERT(!context.aml_data.empty());
		ASSERT(static_cast<AML::Byte>(context.aml_data[0]) == AML::Byte::RefOfOp);
		context.aml_data = context.aml_data.slice(1);

		auto [type, target] = TRY(parse_super_name(context, true));
		if (type == TargetType::Debug)
		{
			dwarnln("TODO: RefOf debug");
			return BAN::Error::from_errno(ENOTSUP);
		}

		Reference* to_reference = target;
		if (target->node.type == Node::Type::Reference)
			to_reference = target->node.as.reference;

		Node result {};
		result.type = Node::Type::Reference;
		result.as.reference = to_reference;
		result.as.reference->ref_count++;
		return result;
	}

	static BAN::ErrorOr<Node> parse_condrefof_op(ParseContext& context)
	{
		dprintln_if(AML_DUMP_FUNCTION_CALLS, "parse_cond_refof_op");

		ASSERT(context.aml_data.size() >= 2);
		ASSERT(static_cast<AML::Byte>(context.aml_data[0]) == AML::Byte::ExtOpPrefix);
		ASSERT(static_cast<AML::ExtOp>(context.aml_data[1]) == AML::ExtOp::CondRefOfOp);
		context.aml_data = context.aml_data.slice(2);

		auto [source_type, source] = TRY(parse_super_name(context, false));
		if (source_type == TargetType::Debug)
		{
			dwarnln("TODO: CondRefOf debug");
			return BAN::Error::from_errno(ENOTSUP);
		}

		if (context.aml_data.empty())
			return BAN::Error::from_errno(ENODATA);

		if (context.aml_data[0] == 0x00)
			context.aml_data = context.aml_data.slice(1);
		else
		{
			auto [target_type, target] = TRY(parse_super_name(context, true));

			if (source)
			{
				Node reference {};
				reference.type = Node::Type::Reference;
				reference.as.reference = source;
				reference.as.reference->ref_count++;
				TRY(perform_store(reference, target, target_type));
			}
		}

		Node result {};
		result.type = Node::Type::Integer;
		result.as.integer.value = source ? ONES : ZERO;
		return result;
	}

	static BAN::ErrorOr<Node> parse_derefof_op(ParseContext& context)
	{
		dprintln_if(AML_DUMP_FUNCTION_CALLS, "parse_derefof_op");

		ASSERT(!context.aml_data.empty());
		ASSERT(static_cast<AML::Byte>(context.aml_data[0]) == AML::Byte::DerefOfOp);
		context.aml_data = context.aml_data.slice(1);

		return derefof_impl(TRY(parse_node(context)));
	}

	static BAN::ErrorOr<void> parse_scope_contents(Scope&& scope, BAN::ConstByteSpan aml_data)
	{
		ParseContext scope_context;
		scope_context.scope      = BAN::move(scope);
		scope_context.aml_data   = aml_data;
		scope_context.call_depth = 0;

		while (!scope_context.aml_data.empty())
		{
			auto parse_result = parse_node_or_execution_flow(scope_context);
			if (parse_result.is_error())
			{
				dwarnln("Failed to parse scope {}: {}", scope_context.scope, parse_result.error());
				return parse_result.release_error();
			}

			auto [execution_flow, node] = parse_result.release_value();
			if (execution_flow != ExecutionFlow::Normal)
			{
				dwarnln("Scope got execution flow {}", static_cast<int>(execution_flow));
				return BAN::Error::from_errno(EINVAL);
			}
		}

		return {};
	}

	static BAN::ErrorOr<void> parse_scope_op(ParseContext& context)
	{
		dprintln_if(AML_DUMP_FUNCTION_CALLS, "parse_scope_op");

		ASSERT(!context.aml_data.empty());
		ASSERT(static_cast<AML::Byte>(context.aml_data[0]) == AML::Byte::ScopeOp);
		context.aml_data = context.aml_data.slice(1);

		auto scope_pkg = TRY(parse_pkg(context.aml_data));
		auto scope_name = TRY(parse_name_string(scope_pkg));

		auto [object_path, object] = TRY(Namespace::root_namespace().find_named_object(context.scope, scope_name));
		if (object == nullptr)
		{
			dwarnln("ScopeOp scope '{}'.'{}' does not exists", context.scope, scope_name);
			return {};
		}

		switch (object->node.type)
		{
			case Node::Type::Device:
			case Node::Type::Processor:
			case Node::Type::PowerResource:
			case Node::Type::ThermalZone:
			case Node::Type::PredefinedScope:
				break;
			default:
				dwarnln("ScopeOp on non-scope object");
				return BAN::Error::from_errno(EINVAL);
		}

		TRY(parse_scope_contents(BAN::move(object_path), scope_pkg));
		return {};
	}

	static BAN::ErrorOr<void> parse_notify_op(ParseContext& context)
	{
		dprintln_if(AML_DUMP_FUNCTION_CALLS, "parse_scope_op");

		ASSERT(!context.aml_data.empty());
		ASSERT(static_cast<AML::Byte>(context.aml_data[0]) == AML::Byte::NotifyOp);
		context.aml_data = context.aml_data.slice(1);

		auto object = TRY(parse_super_name(context, true));
		auto value = TRY(convert_node(TRY(parse_node(context)), ConvInteger, sizeof(uint64_t)));

		dwarnln("TODO: handle notify({}, {})", object.elem2->node, value);

		return {};
	}

	static BAN::ErrorOr<void> parse_event_op(ParseContext& context)
	{
		dprintln_if(AML_DUMP_FUNCTION_CALLS, "parse_device_op");

		ASSERT(context.aml_data.size() >= 2);
		ASSERT(static_cast<AML::Byte>(context.aml_data[0]) == AML::Byte::ExtOpPrefix);
		ASSERT(static_cast<AML::ExtOp>(context.aml_data[1]) == AML::ExtOp::EventOp);
		context.aml_data = context.aml_data.slice(2);

		auto event_name = TRY(parse_name_string(context.aml_data));

		Node event_node {};
		event_node.type = Node::Type::Event;
		event_node.as.event.signal_count = 0;

		auto absolute_path = TRY(Namespace::root_namespace().add_named_object(context.scope, event_name, BAN::move(event_node)));
		if (absolute_path.parts.empty())
		{
			dwarnln("Could not add Device '{}'.'{}' to namespace", context.scope, event_name);
			return {};
		}

		return {};
	}

	static BAN::ErrorOr<void> parse_reset_signal_op(ParseContext& context)
	{
		dprintln_if(AML_DUMP_FUNCTION_CALLS, "parse_reset_signal_op");

		ASSERT(context.aml_data.size() >= 2);
		ASSERT(static_cast<AML::Byte>(context.aml_data[0]) == AML::Byte::ExtOpPrefix);
		const auto opcode = static_cast<AML::ExtOp>(context.aml_data[1]);
		context.aml_data = context.aml_data.slice(2);

		auto [_, event_ref] = TRY(parse_super_name(context, true));
		if (event_ref == nullptr)
		{
			dwarnln("ResetOp/SignalOp event is null");
			return BAN::Error::from_errno(EINVAL);
		}
		if (event_ref->node.type != Node::Type::Event)
		{
			dwarnln("ResetOp/SignalOp Object '{}' is not an event", event_ref->node);
			return BAN::Error::from_errno(EINVAL);
		}

		switch (opcode)
		{
			case AML::ExtOp::ResetOp:
				event_ref->node.as.event.signal_count = 0;
				break;
			case AML::ExtOp::SignalOp:
				event_ref->node.as.event.signal_count++;
				break;
			default:
				ASSERT_NOT_REACHED();
		}

		return {};
	}


	static BAN::ErrorOr<Node> parse_wait_op(ParseContext& context)
	{
		dprintln_if(AML_DUMP_FUNCTION_CALLS, "parse_wait_op");

		ASSERT(context.aml_data.size() >= 2);
		ASSERT(static_cast<AML::Byte>(context.aml_data[0]) == AML::Byte::ExtOpPrefix);
		ASSERT(static_cast<AML::ExtOp>(context.aml_data[1]) == AML::ExtOp::WaitOp);
		context.aml_data = context.aml_data.slice(2);

		auto [_, event_ref] = TRY(parse_super_name(context, true));
		if (event_ref == nullptr)
		{
			dwarnln("WaitOp event is null");
			return BAN::Error::from_errno(EINVAL);
		}
		if (event_ref->node.type != Node::Type::Event)
		{
			dwarnln("WaitOp Object '{}' is not an event", event_ref->node);
			return BAN::Error::from_errno(EINVAL);
		}

		const uint64_t timeout_ms = TRY(convert_node(TRY(parse_node(context)), ConvInteger, sizeof(uint64_t))).as.integer.value;
		const uint64_t wake_time_ms = (timeout_ms >= 0xFFFF) ? BAN::numeric_limits<uint64_t>::max() : SystemTimer::get().ms_since_boot() + timeout_ms;

		uint64_t return_value = 0;
		for (;;)
		{
			if (event_ref->node.as.event.signal_count > 0)
			{
				event_ref->node.as.event.signal_count--;
				return_value = ZERO;
				break;
			}

			if (wake_time_ms >= SystemTimer::get().ms_since_boot())
			{
				return_value = ONES;
				break;
			}

			Processor::yield();
		}

		Node result;
		result.type = Node::Type::Integer;
		result.as.integer.value = return_value;
		return result;
	}

	static BAN::ErrorOr<void> parse_sleep_op(ParseContext& context)
	{
		dprintln_if(AML_DUMP_FUNCTION_CALLS, "parse_sleep_op");

		ASSERT(context.aml_data.size() >= 2);
		ASSERT(static_cast<AML::Byte>(context.aml_data[0]) == AML::Byte::ExtOpPrefix);
		ASSERT(static_cast<AML::ExtOp>(context.aml_data[1]) == AML::ExtOp::SleepOp);
		context.aml_data = context.aml_data.slice(2);

		auto milliseconds = TRY(convert_node(TRY(parse_node(context)), ConvInteger, sizeof(uint64_t)));

		const uint64_t wakeup_ms = SystemTimer::get().ms_since_boot() + milliseconds.as.integer.value;
		for (uint64_t curr_ms = SystemTimer::get().ms_since_boot(); curr_ms < wakeup_ms; curr_ms = SystemTimer::get().ms_since_boot())
			SystemTimer::get().sleep_ms(wakeup_ms - curr_ms);

		return {};
	}

	static BAN::ErrorOr<void> parse_stall_op(ParseContext& context)
	{
		dprintln_if(AML_DUMP_FUNCTION_CALLS, "parse_stall_op");

		ASSERT(context.aml_data.size() >= 2);
		ASSERT(static_cast<AML::Byte>(context.aml_data[0]) == AML::Byte::ExtOpPrefix);
		ASSERT(static_cast<AML::ExtOp>(context.aml_data[1]) == AML::ExtOp::StallOp);
		context.aml_data = context.aml_data.slice(2);

		const auto microseconds = TRY(convert_node(TRY(parse_node(context)), ConvInteger, sizeof(uint64_t))).as.integer.value;

		if (microseconds > 100)
		{
			dwarnln("Stall for {} us", microseconds);
			return BAN::Error::from_errno(EINVAL);
		}

		const uint64_t wakeup_ns = SystemTimer::get().ns_since_boot() + microseconds * 1000;
		while (SystemTimer::get().ns_since_boot() < wakeup_ns)
			Processor::pause();

		return {};
	}

	static BAN::ErrorOr<void> parse_device_op(ParseContext& context)
	{
		dprintln_if(AML_DUMP_FUNCTION_CALLS, "parse_device_op");

		ASSERT(context.aml_data.size() >= 2);
		ASSERT(static_cast<AML::Byte>(context.aml_data[0]) == AML::Byte::ExtOpPrefix);
		ASSERT(static_cast<AML::ExtOp>(context.aml_data[1]) == AML::ExtOp::DeviceOp);
		context.aml_data = context.aml_data.slice(2);

		auto device_pkg = TRY(parse_pkg(context.aml_data));
		auto device_name = TRY(parse_name_string(device_pkg));

		Node device_node {};
		device_node.type = Node::Type::Device;

		auto absolute_path = TRY(Namespace::root_namespace().add_named_object(context.scope, device_name, BAN::move(device_node)));
		if (absolute_path.parts.empty())
		{
			dwarnln("Could not add Device '{}'.'{}' to namespace", context.scope, device_name);
			return {};
		}

		TRY(parse_scope_contents(BAN::move(absolute_path), device_pkg));
		return {};
	}

	static BAN::ErrorOr<void> parse_processor_op(ParseContext& context)
	{
		dprintln_if(AML_DUMP_FUNCTION_CALLS, "parse_processor_op");

		ASSERT(context.aml_data.size() >= 2);
		ASSERT(static_cast<AML::Byte>(context.aml_data[0]) == AML::Byte::ExtOpPrefix);
		ASSERT(static_cast<AML::ExtOp>(context.aml_data[1]) == AML::ExtOp::ProcessorOp);
		context.aml_data = context.aml_data.slice(2);

		auto processor_pkg = TRY(parse_pkg(context.aml_data));
		auto processor_name = TRY(parse_name_string(processor_pkg));

		// FIXME: do something with ProcID, PblkAddr, PblkLen?
		if (processor_pkg.size() < 6)
			return BAN::Error::from_errno(ENODATA);
		processor_pkg = processor_pkg.slice(6);

		Node processor_node {};
		processor_node.type = Node::Type::Processor;

		auto absolute_path = TRY(Namespace::root_namespace().add_named_object(context.scope, processor_name, BAN::move(processor_node)));
		if (absolute_path.parts.empty())
		{
			dwarnln("Could not add Processor '{}'.'{}' to namespace", context.scope, processor_name);
			return {};
		}

		TRY(parse_scope_contents(BAN::move(absolute_path), processor_pkg));
		return {};
	}

	static BAN::ErrorOr<void> parse_power_resource_op(ParseContext& context)
	{
		dprintln_if(AML_DUMP_FUNCTION_CALLS, "parse_power_resource_op");

		ASSERT(context.aml_data.size() >= 2);
		ASSERT(static_cast<AML::Byte>(context.aml_data[0]) == AML::Byte::ExtOpPrefix);
		ASSERT(static_cast<AML::ExtOp>(context.aml_data[1]) == AML::ExtOp::PowerResOp);
		context.aml_data = context.aml_data.slice(2);

		auto power_resource_pkg = TRY(parse_pkg(context.aml_data));
		auto power_resource_name = TRY(parse_name_string(power_resource_pkg));

		// FIXME: do something with SystemLevel, ResourceOrder?
		if (power_resource_pkg.size() < 3)
			return BAN::Error::from_errno(ENODATA);
		power_resource_pkg = power_resource_pkg.slice(3);

		Node power_resource_node {};
		power_resource_node.type = Node::Type::PowerResource;

		auto absolute_path = TRY(Namespace::root_namespace().add_named_object(context.scope, power_resource_name, BAN::move(power_resource_node)));
		if (absolute_path.parts.empty())
		{
			dwarnln("Could not add Processor '{}'.'{}' to namespace", context.scope, power_resource_name);
			return {};
		}

		TRY(parse_scope_contents(BAN::move(absolute_path), power_resource_pkg));
		return {};
	}

	static BAN::ErrorOr<void> parse_thermal_zone_op(ParseContext& context)
	{
		dprintln_if(AML_DUMP_FUNCTION_CALLS, "parse_thermal_zone_op");

		ASSERT(context.aml_data.size() >= 2);
		ASSERT(static_cast<AML::Byte>(context.aml_data[0]) == AML::Byte::ExtOpPrefix);
		ASSERT(static_cast<AML::ExtOp>(context.aml_data[1]) == AML::ExtOp::ThermalZoneOp);
		context.aml_data = context.aml_data.slice(2);

		auto thermal_zone_pkg = TRY(parse_pkg(context.aml_data));
		auto thermal_zone_name = TRY(parse_name_string(thermal_zone_pkg));

		Node thermal_zone_node {};
		thermal_zone_node.type = Node::Type::ThermalZone;

		auto absolute_path = TRY(Namespace::root_namespace().add_named_object(context.scope, thermal_zone_name, BAN::move(thermal_zone_node)));
		if (absolute_path.parts.empty())
		{
			dwarnln("Could not add Thermal Zone '{}'.'{}' to namespace", context.scope, thermal_zone_name);
			return {};
		}

		TRY(parse_scope_contents(BAN::move(absolute_path), thermal_zone_pkg));
		return {};
	}

	static BAN::ErrorOr<void> parse_mutex_op(ParseContext& context)
	{
		dprintln_if(AML_DUMP_FUNCTION_CALLS, "parse_mutex_op");

		ASSERT(context.aml_data.size() >= 2);
		ASSERT(static_cast<AML::Byte>(context.aml_data[0]) == AML::Byte::ExtOpPrefix);
		ASSERT(static_cast<AML::ExtOp>(context.aml_data[1]) == AML::ExtOp::MutexOp);
		context.aml_data = context.aml_data.slice(2);

		auto mutex_name = TRY(parse_name_string(context.aml_data));
		if (context.aml_data.empty())
			return BAN::Error::from_errno(ENODATA);
		const uint8_t mutex_flags = context.aml_data[0];
		context.aml_data = context.aml_data.slice(1);

		if (mutex_flags & 0xF0)
		{
			dwarnln("MutexOp flags has bits in top nibble set");
			return BAN::Error::from_errno(EINVAL);
		}

		Node mutex;
		mutex.type = Node::Type::Mutex;
		mutex.as.mutex = new Mutex();
		if (mutex.as.mutex == nullptr)
			return BAN::Error::from_errno(ENOMEM);
		mutex.as.mutex->sync_level = mutex_flags;
		mutex.as.mutex->global_lock = false;

		TRY(Namespace::root_namespace().add_named_object(context.scope, mutex_name, BAN::move(mutex)));
		return {};
	}

	static BAN::ErrorOr<void> parse_fatal_op(ParseContext& context)
	{
		dprintln_if(AML_DUMP_FUNCTION_CALLS, "parse_fatal_op");

		ASSERT(context.aml_data.size() >= 2);
		ASSERT(static_cast<AML::Byte>(context.aml_data[0]) == AML::Byte::ExtOpPrefix);
		ASSERT(static_cast<AML::ExtOp>(context.aml_data[1]) == AML::ExtOp::FatalOp);
		context.aml_data = context.aml_data.slice(2);

		if (context.aml_data.size() < 5)
			return BAN::Error::from_errno(ENODATA);

		const uint8_t fatal_type = context.aml_data[0];

		const uint32_t fatal_code = 0
			| ((uint32_t)context.aml_data[1] <<  0)
			| ((uint32_t)context.aml_data[2] <<  8)
			| ((uint32_t)context.aml_data[3] << 16)
			| ((uint32_t)context.aml_data[4] << 24);

		context.aml_data = context.aml_data.slice(5);

		const auto fatal_arg = TRY(parse_node(context));

		derrorln("FATAL: type {2H}, code {8H}, arg {}", fatal_type, fatal_code, fatal_arg);

		return {};
	}

	static BAN::ErrorOr<Node> parse_acquire_op(ParseContext& context)
	{
		dprintln_if(AML_DUMP_FUNCTION_CALLS, "parse_acquire_op");

		ASSERT(context.aml_data.size() >= 2);
		ASSERT(static_cast<AML::Byte>(context.aml_data[0]) == AML::Byte::ExtOpPrefix);
		ASSERT(static_cast<AML::ExtOp>(context.aml_data[1]) == AML::ExtOp::AcquireOp);
		context.aml_data = context.aml_data.slice(2);

		auto [type, sync_object] = TRY(parse_super_name(context, true));
		if (type == TargetType::Debug)
		{
			dwarnln("AquireOp sync object is Debug");
			return BAN::Error::from_errno(EINVAL);
		}

		ASSERT(sync_object);
		if (sync_object->node.type != Node::Type::Mutex)
		{
			dwarnln("AquireOp sync object is {}", sync_object->node);
			return BAN::Error::from_errno(EINVAL);
		}

		if (context.aml_data.size() < 2)
			return BAN::Error::from_errno(ENODATA);

		const uint16_t timeout_ms = context.aml_data.as<const uint16_t>();
		context.aml_data = context.aml_data.slice(2);

		Node result;
		result.type = Node::Type::Integer;
		result.as.integer.value = BAN::numeric_limits<uint64_t>::max();

		const uint64_t wake_time_ms = (timeout_ms >= 0xFFFF)
			? BAN::numeric_limits<uint64_t>::max()
			: SystemTimer::get().ms_since_boot() + timeout_ms;

		result.as.integer.value = 0;
		while (true)
		{
			if (sync_object->node.as.mutex->mutex.try_lock())
				break;
			if (SystemTimer::get().ms_since_boot() >= wake_time_ms)
				return result;
			SystemTimer::get().sleep_ms(1);
		}

		if (sync_object->node.as.mutex->global_lock)
			ACPI::acquire_global_lock();

		result.as.integer.value = 0;
		return result;
	}

	static BAN::ErrorOr<void> parse_release_op(ParseContext& context)
	{
		dprintln_if(AML_DUMP_FUNCTION_CALLS, "parse_release_op");

		ASSERT(context.aml_data.size() >= 2);
		ASSERT(static_cast<AML::Byte>(context.aml_data[0]) == AML::Byte::ExtOpPrefix);
		ASSERT(static_cast<AML::ExtOp>(context.aml_data[1]) == AML::ExtOp::ReleaseOp);
		context.aml_data = context.aml_data.slice(2);

		auto [type, sync_object] = TRY(parse_super_name(context, true));
		if (type == TargetType::Debug)
		{
			dwarnln("AquireOp sync object is Debug");
			return BAN::Error::from_errno(EINVAL);
		}

		ASSERT(sync_object);
		if (sync_object->node.type != Node::Type::Mutex)
		{
			dwarnln("AquireOp sync object is {}", sync_object->node);
			return BAN::Error::from_errno(EINVAL);
		}

		if (sync_object->node.as.mutex->global_lock)
			ACPI::release_global_lock();

		sync_object->node.as.mutex->mutex.unlock();

		return {};
	}

	static BAN::ErrorOr<void> parse_method_op(ParseContext& context)
	{
		dprintln_if(AML_DUMP_FUNCTION_CALLS, "parse_method_op");

		ASSERT(!context.aml_data.empty());
		ASSERT(static_cast<AML::Byte>(context.aml_data[0]) == AML::Byte::MethodOp);
		context.aml_data = context.aml_data.slice(1);

		auto method_pkg = TRY(parse_pkg(context.aml_data));
		auto method_name = TRY(parse_name_string(method_pkg));
		if (method_pkg.empty())
			return BAN::Error::from_errno(ENODATA);
		const uint8_t method_flags = method_pkg[0];
		method_pkg = method_pkg.slice(1);

		Node method_node {};
		method_node.type = Node::Type::Method;
		method_node.as.method.start      = method_pkg.data();
		method_node.as.method.length     = method_pkg.size();
		method_node.as.method.arg_count  = method_flags & 0x07;
		method_node.as.method.serialized = !!(method_flags & 0x80);
		method_node.as.method.mutex      = nullptr;
		if (method_node.as.method.serialized)
		{
			method_node.as.method.mutex = new Mutex();
			method_node.as.method.mutex->sync_level  = method_flags >> 4;
			method_node.as.method.mutex->global_lock = false;
			method_node.as.method.mutex->ref_count   = 1;
		}

		auto path = TRY(Namespace::root_namespace().add_named_object(context.scope, method_name, BAN::move(method_node)));
		if (!path.parts.empty())
			TRY(context.created_nodes.push_back(BAN::move(path)));

		return {};
	}

	static BAN::ErrorOr<ExecutionFlowResult> parse_if_op(ParseContext& context)
	{
		dprintln_if(AML_DUMP_FUNCTION_CALLS, "parse_if_op");

		ASSERT(!context.aml_data.empty());
		ASSERT(static_cast<AML::Byte>(context.aml_data[0]) == AML::Byte::IfOp);
		context.aml_data = context.aml_data.slice(1);

		auto if_pkg = TRY(parse_pkg(context.aml_data));

		auto old_aml_data = context.aml_data;
		context.aml_data = if_pkg;

		auto predicate_node = TRY(convert_node(TRY(parse_node(context)), ConvInteger, sizeof(uint64_t)));
		const bool predicate = predicate_node.as.integer.value;

		BAN::ConstByteSpan code_path;
		if (predicate)
			code_path = context.aml_data;

		if (!old_aml_data.empty() && static_cast<AML::Byte>(old_aml_data[0]) == AML::Byte::ElseOp)
		{
			old_aml_data = old_aml_data.slice(1);
			auto else_pkg = TRY(parse_pkg(old_aml_data));
			if (!predicate)
				code_path = else_pkg;
		}

		Node return_value {};
		ExecutionFlow execution_flow = ExecutionFlow::Normal;

		context.aml_data = code_path;
		while (!context.aml_data.empty() && execution_flow == ExecutionFlow::Normal)
		{
			auto parse_result = parse_node_or_execution_flow(context);
			if (parse_result.is_error())
			{
				dwarnln("Failed to parse if body in {}: {}", context.scope, parse_result.error());
				return parse_result.release_error();
			}

			auto [new_execution_flow, node] = parse_result.release_value();
			execution_flow = new_execution_flow;

			switch (execution_flow)
			{
				case ExecutionFlow::Normal:
					break;
				case ExecutionFlow::Break:
					break;
				case ExecutionFlow::Continue:
					break;
				case ExecutionFlow::Return:
					ASSERT(node.has_value());
					return_value = node.release_value();
					break;
			}
		}
		context.aml_data = old_aml_data;

		return ExecutionFlowResult {
			.elem1 = execution_flow,
			.elem2 = BAN::move(return_value),
		};
	}

	static BAN::ErrorOr<ExecutionFlowResult> parse_while_op(ParseContext& context)
	{
		dprintln_if(AML_DUMP_FUNCTION_CALLS, "parse_while_op");

		ASSERT(!context.aml_data.empty());
		ASSERT(static_cast<AML::Byte>(context.aml_data[0]) == AML::Byte::WhileOp);
		context.aml_data = context.aml_data.slice(1);

		auto while_pkg = TRY(parse_pkg(context.aml_data));

		auto old_aml_data = context.aml_data;

		Node return_value {};
		ExecutionFlow execution_flow = ExecutionFlow::Normal;

		const uint64_t while_loop_start_ms = SystemTimer::get().ms_since_boot();
		while (execution_flow == ExecutionFlow::Normal)
		{
			if (auto current_ms = SystemTimer::get().ms_since_boot(); current_ms >= while_loop_start_ms + 5000)
			{
				dwarnln("While loop terminated after {} ms", current_ms - while_loop_start_ms);
				break;
			}

			context.aml_data = while_pkg;

			auto predicate = TRY(convert_node(TRY(parse_node(context)), ConvInteger, sizeof(uint64_t)));
			if (predicate.as.integer.value == 0)
				break;

			while (!context.aml_data.empty() && execution_flow == ExecutionFlow::Normal)
			{
				auto parse_result = parse_node_or_execution_flow(context);
				if (parse_result.is_error())
				{
					dwarnln("Failed to parse while body in {}: {}", context.scope, parse_result.error());
					return parse_result.release_error();
				}

				auto [new_execution_flow, node] = parse_result.release_value();
				execution_flow = new_execution_flow;

				switch (execution_flow)
				{
					case ExecutionFlow::Normal:
						break;
					case ExecutionFlow::Break:
						break;
					case ExecutionFlow::Continue:
						break;
					case ExecutionFlow::Return:
						ASSERT(node.has_value());
						return_value = node.release_value();
						break;
				}
			}

			if (execution_flow == ExecutionFlow::Continue)
				execution_flow = ExecutionFlow::Normal;
		}

		context.aml_data = old_aml_data;

		if (execution_flow == ExecutionFlow::Break)
			execution_flow = ExecutionFlow::Normal;

		return ExecutionFlowResult {
			.elem1 = execution_flow,
			.elem2 = BAN::move(return_value),
		};
	}

	static BAN::ErrorOr<Node> parse_load_op(ParseContext& context)
	{
		dprintln_if(AML_DUMP_FUNCTION_CALLS, "parse_load_op");

		ASSERT(context.aml_data.size() >= 2);
		ASSERT(static_cast<AML::Byte>(context.aml_data[0]) == AML::Byte::ExtOpPrefix);
		ASSERT(static_cast<AML::ExtOp>(context.aml_data[1]) == AML::ExtOp::LoadOp);
		context.aml_data = context.aml_data.slice(2);

		auto load_name = TRY(parse_name_string(context.aml_data));
		auto [named_object_path, named_object] = TRY(Namespace::root_namespace().find_named_object(context.scope, load_name));

		Node result;
		result.type = Node::Type::Integer;
		result.as.integer.value = 0;

		if (named_object == nullptr)
			dwarnln("Load target does not exist");
		else
		{
			switch (named_object->node.type)
			{
				case Node::Type::Buffer:
				{
					auto data = BAN::ConstByteSpan(named_object->node.as.str_buf->bytes, named_object->node.as.str_buf->size);
					if (auto ret = Namespace::root_namespace().parse(data); ret.is_error())
						dwarnln("Failed to load {}: {}", named_object->node, ret.error());
					else
						result.as.integer.value = BAN::numeric_limits<uint64_t>::max();
					break;
				}
				default:
					dwarnln("TODO: Load({}): {}", named_object_path, named_object->node);
					return BAN::Error::from_errno(ENOTSUP);
			}
		}

		TRY(store_into_target(context, result));

		return result;
	}

	BAN::ErrorOr<Node> parse_timer_op(ParseContext& context)
	{
		dprintln_if(AML_DUMP_FUNCTION_CALLS, "parse_load_op");

		ASSERT(context.aml_data.size() >= 2);
		ASSERT(static_cast<AML::Byte>(context.aml_data[0]) == AML::Byte::ExtOpPrefix);
		ASSERT(static_cast<AML::ExtOp>(context.aml_data[1]) == AML::ExtOp::TimerOp);
		context.aml_data = context.aml_data.slice(2);

		Node result;
		result.type = Node::Type::Integer;
		result.as.integer.value = SystemTimer::get().ns_since_boot() / 100;
		return result;
	}

	BAN::ErrorOr<Node> evaluate_node(const Scope& node_path, const Node& node)
	{
		switch (node.type)
		{
			case Node::Type::Uninitialized:
			case Node::Type::Debug:
			case Node::Type::OpRegion:
			case Node::Type::Event:
			case Node::Type::Device:
			case Node::Type::Processor:
			case Node::Type::PowerResource:
			case Node::Type::ThermalZone:
			case Node::Type::Mutex:
			case Node::Type::PredefinedScope:
			case Node::Type::Count:
				break;
			case Node::Type::Integer:
			case Node::Type::String:
			case Node::Type::Package:
			case Node::Type::Buffer:
			case Node::Type::Index:
			case Node::Type::Reference:
				return TRY(node.copy());
			case Node::Type::BufferField:
				dwarnln("TODO: evaluate BufferField");
				return BAN::Error::from_errno(ENOTSUP);
			case Node::Type::FieldUnit:
				return convert_from_field_unit(node, ConvInteger | ConvBuffer, sizeof(uint64_t));
			case Node::Type::Method:
				if (node.as.method.arg_count != 0)
					return BAN::Error::from_errno(EFAULT);
				return TRY(method_call(node_path, node, {}));
		}

		dwarnln("evaluate {}", node);
		return BAN::Error::from_errno(EINVAL);
	}

	static BAN::ErrorOr<Node> method_call_impl(ParseContext& context)
	{
		dprintln_if(AML_DUMP_FUNCTION_CALLS, "method_call '{}'", context.scope);

		if (context.call_depth >= 128)
		{
			dwarnln("Terminating recursive method call");

			Node return_value {};
			return_value.type = Node::Type::Integer;
			return_value.as.integer.value = 0;
			return return_value;
		}

		TRY(context.allocate_locals());

		Node return_value {};

		context.call_depth++;
		while (!context.aml_data.empty() && return_value.type == Node::Type::Uninitialized)
		{
			auto [execution_flow, node] = TRY(parse_node_or_execution_flow(context));

			switch (execution_flow)
			{
				case ExecutionFlow::Normal:
					break;
				case ExecutionFlow::Break:
					dwarnln("BreakOp in method");
					return BAN::Error::from_errno(EINVAL);
				case ExecutionFlow::Continue:
					dwarnln("ContinueOp in method");
					return BAN::Error::from_errno(EINVAL);
				case ExecutionFlow::Return:
					ASSERT(node.has_value());
					return_value = node.release_value();
					break;
			}
		}
		context.call_depth--;

		while (!context.created_nodes.empty())
		{
			TRY(Namespace::root_namespace().remove_named_object(context.created_nodes.back()));
			context.created_nodes.pop_back();
		}

		// In the absence of an explicit Return () statement, the return value to the caller is undefined.
		// We will just return 0 to keep things simple :)
		if (return_value.type == Node::Type::Uninitialized)
		{
			return_value.type = Node::Type::Integer;
			return_value.as.integer.value = 0;
		}

		return return_value;
	}

	BAN::ErrorOr<Node> method_call(const Scope& scope, const Node& method_node, BAN::Array<Reference*, 7>&& args, uint32_t call_depth)
	{
		ASSERT(method_node.type == Node::Type::Method);

		size_t argc = 0;
		for (argc = 0; argc < args.size(); argc++)
			if (args[argc] == nullptr)
				break;
		for (size_t i = argc; i < args.size(); i++)
			ASSERT(args[i] == nullptr);

		const auto& method = method_node.as.method;

		if (argc != method.arg_count)
		{
			dwarnln("{} takes {} arguments but {} were provided", scope, method.arg_count, argc);
			return BAN::Error::from_errno(EINVAL);
		}

		if (method.override_func)
			return method.override_func(args);

		if (method.serialized)
		{
			ASSERT(method.mutex);
			method.mutex->mutex.lock();
		}

		// FIXME: I'm pretty sure arguments are leaking memory

		ParseContext context;
		context.scope = TRY(scope.copy());
		context.aml_data = BAN::ConstByteSpan(method.start, method.length);
		context.call_depth = call_depth;
		for (size_t i = 0; i < args.size(); i++)
			context.args[i] = args[i];

		auto result = method_call_impl(context);
		if (method.serialized)
			method.mutex->mutex.unlock();

		return result;
	}

	BAN::ErrorOr<Node> parse_node(ParseContext& context, bool return_ref)
	{
		if (context.aml_data.empty())
			return BAN::Error::from_errno(ENODATA);

		if (context.aml_data[0] == static_cast<uint8_t>(AML::Byte::ExtOpPrefix))
		{
			if (context.aml_data.size() < 2)
				return BAN::Error::from_errno(ENODATA);

			const uint8_t opcode = context.aml_data[1];
			switch (static_cast<AML::ExtOp>(opcode))
			{
				case AML::ExtOp::CondRefOfOp:
					return TRY(parse_condrefof_op(context));
				case AML::ExtOp::AcquireOp:
					return TRY(parse_acquire_op(context));
				case AML::ExtOp::LoadOp:
					return TRY(parse_load_op(context));
				case AML::ExtOp::TimerOp:
					return TRY(parse_timer_op(context));
				case AML::ExtOp::WaitOp:
					return TRY(parse_wait_op(context));
				case AML::ExtOp::DebugOp:
				{
					context.aml_data = context.aml_data.slice(2);
					Node debug;
					debug.type = Node::Type::Debug;
					return BAN::move(debug);
				}
				default:
					break;
			}

			dwarnln("TODO: AML opcode {2H}e", opcode);
			return BAN::Error::from_errno(ENOTSUP);
		}

		const uint8_t opcode = context.aml_data[0];
		switch (static_cast<AML::Byte>(opcode))
		{
			case AML::Byte::ZeroOp:
			case AML::Byte::OneOp:
			case AML::Byte::OnesOp:
			case AML::Byte::BytePrefix:
			case AML::Byte::WordPrefix:
			case AML::Byte::DWordPrefix:
			case AML::Byte::QWordPrefix:
				return TRY(parse_integer(context.aml_data));
			case AML::Byte::StringPrefix:
				return TRY(parse_string(context.aml_data));
			case AML::Byte::BufferOp:
				return TRY(parse_buffer_op(context));
			case AML::Byte::PackageOp:
			case AML::Byte::VarPackageOp:
				return TRY(parse_package_op(context));
			case AML::Byte::SizeOfOp:
				return TRY(parse_sizeof_op(context));
			case AML::Byte::RefOfOp:
				return TRY(parse_refof_op(context));
			case AML::Byte::DerefOfOp:
				return TRY(parse_derefof_op(context));
			case AML::Byte::StoreOp:
				return TRY(parse_store_op(context));
			case AML::Byte::CopyObjectOp:
				return TRY(parse_copy_object_op(context));
			case AML::Byte::ConcatOp:
				return TRY(parse_concat_op(context));
			case AML::Byte::MidOp:
				return TRY(parse_mid_op(context));
			case AML::Byte::IndexOp:
				return TRY(parse_index_op(context));
			case AML::Byte::ObjectTypeOp:
				return TRY(parse_object_type_op(context));
			case AML::Byte::ToBufferOp:
			case AML::Byte::ToDecimalStringOp:
			case AML::Byte::ToHexStringOp:
			case AML::Byte::ToIntegerOp:
				return TRY(parse_explicit_conversion(context));
			case AML::Byte::ToStringOp:
				return TRY(parse_to_string_op(context));
			case AML::Byte::IncrementOp:
			case AML::Byte::DecrementOp:
				return TRY(parse_inc_dec_op(context));
			case AML::Byte::NotOp:
				return TRY(parse_unary_integer_op(context));
			case AML::Byte::AddOp:
			case AML::Byte::SubtractOp:
			case AML::Byte::MultiplyOp:
			case AML::Byte::DivideOp:
			case AML::Byte::ShiftLeftOp:
			case AML::Byte::ShiftRightOp:
			case AML::Byte::AndOp:
			case AML::Byte::NandOp:
			case AML::Byte::OrOp:
			case AML::Byte::NorOp:
			case AML::Byte::XorOp:
			case AML::Byte::ModOp:
				return TRY(parse_binary_integer_op(context));
			case AML::Byte::LAndOp:
			case AML::Byte::LEqualOp:
			case AML::Byte::LGreaterOp:
			case AML::Byte::LLessOp:
			case AML::Byte::LNotOp:
			case AML::Byte::LOrOp:
				return TRY(parse_logical_op(context));
			case AML::Byte::Local0:
			case AML::Byte::Local1:
			case AML::Byte::Local2:
			case AML::Byte::Local3:
			case AML::Byte::Local4:
			case AML::Byte::Local5:
			case AML::Byte::Local6:
			case AML::Byte::Local7:
			{
				const uint8_t local_index = opcode - static_cast<uint8_t>(AML::Byte::Local0);
				context.aml_data = context.aml_data.slice(1);
				if (context.locals[local_index]->node.type == Node::Type::Uninitialized)
				{
					dwarnln("Trying to access uninitialized local");
					return BAN::Error::from_errno(EINVAL);
				}
				if (!return_ref)
					return TRY(context.locals[local_index]->node.copy());
				Node reference;
				reference.type = Node::Type::Reference;
				reference.as.reference = context.locals[local_index];
				reference.as.reference->ref_count++;
				return reference;
			}
			case AML::Byte::Arg0:
			case AML::Byte::Arg1:
			case AML::Byte::Arg2:
			case AML::Byte::Arg3:
			case AML::Byte::Arg4:
			case AML::Byte::Arg5:
			case AML::Byte::Arg6:
			{
				const uint8_t arg_index = opcode - static_cast<uint8_t>(AML::Byte::Arg0);
				context.aml_data = context.aml_data.slice(1);
				if (context.args[arg_index] == nullptr || context.args[arg_index]->node.type == Node::Type::Uninitialized)
				{
					dwarnln("Trying to access uninitialized arg");
					return BAN::Error::from_errno(EINVAL);
				}
				if (!return_ref)
					return TRY(context.args[arg_index]->node.copy());
				Node reference;
				reference.type = Node::Type::Reference;
				reference.as.reference = context.args[arg_index];
				reference.as.reference->ref_count++;
				return reference;
			}
			default:
				break;
		}

		if (!is_name_string_start(context.aml_data[0]))
		{
			dwarnln("TODO: AML opcode {2H}", opcode);
			return BAN::Error::from_errno(ENOTSUP);
		}

		auto name_string = TRY(parse_name_string(context.aml_data));

		auto [object_scope, named_object] = TRY(Namespace::root_namespace().find_named_object(context.scope, name_string));
		if (named_object == nullptr)
		{
			dwarnln("could not find '{}'.'{}'", context.scope, name_string);
			return BAN::Error::from_errno(ENOENT);
		}

		if (named_object->node.type == Node::Type::Method)
		{
			BAN::Array<Reference*, 7> args;
			for (size_t i = 0; i < named_object->node.as.method.arg_count; i++)
			{
				auto temp = TRY(parse_node(context));
				if (temp.type == Node::Type::Reference)
				{
					args[i] = temp.as.reference;
					args[i]->ref_count++;
				}
				else
				{
					args[i] = new Reference();
					if (args[i] == nullptr)
						return BAN::Error::from_errno(ENOMEM);
					args[i]->node = BAN::move(temp);
					args[i]->ref_count = 1;
				}
			}

			return TRY(method_call(BAN::move(object_scope), named_object->node, BAN::move(args), context.call_depth));
		}

		if (!return_ref)
			return TRY(named_object->node.copy());

		Node reference;
		reference.type = Node::Type::Reference;
		reference.as.reference = named_object;
		reference.as.reference->ref_count++;
		return reference;
	}

// FIXME: WHY TF IS THIS USING ALMOST 2 KiB of stack
#pragma GCC diagnostic push
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic ignored "-Wstack-usage="
#endif

	BAN::ErrorOr<ExecutionFlowResult> parse_node_or_execution_flow(ParseContext& context)
	{
		if (context.aml_data.empty())
			return BAN::Error::from_errno(ENODATA);

		auto dummy_return = ExecutionFlowResult {
			.elem1 = ExecutionFlow::Normal,
			.elem2 = BAN::Optional<Node>(),
		};

		if (context.aml_data[0] == static_cast<uint8_t>(AML::Byte::ExtOpPrefix))
		{
			if (context.aml_data.size() < 2)
				return BAN::Error::from_errno(ENODATA);
			switch (static_cast<AML::ExtOp>(context.aml_data[1]))
			{
				case AML::ExtOp::MutexOp:
					TRY(parse_mutex_op(context));
					return dummy_return;
				case AML::ExtOp::FatalOp:
					TRY(parse_fatal_op(context));
					return dummy_return;
				case AML::ExtOp::EventOp:
					TRY(parse_event_op(context));
					return dummy_return;
				case AML::ExtOp::ResetOp:
				case AML::ExtOp::SignalOp:
					TRY(parse_reset_signal_op(context));
					return dummy_return;
				case AML::ExtOp::CreateFieldOp:
					TRY(parse_createfield_op(context));
					return dummy_return;
				case AML::ExtOp::SleepOp:
					TRY(parse_sleep_op(context));
					return dummy_return;
				case AML::ExtOp::StallOp:
					TRY(parse_stall_op(context));
					return dummy_return;
				case AML::ExtOp::ReleaseOp:
					TRY(parse_release_op(context));
					return dummy_return;
				case AML::ExtOp::OpRegionOp:
					TRY(parse_opregion_op(context));
					return dummy_return;
				case AML::ExtOp::FieldOp:
					TRY(parse_field_op(context));
					return dummy_return;
				case AML::ExtOp::IndexFieldOp:
					TRY(parse_index_field_op(context));
					return dummy_return;
				case AML::ExtOp::BankFieldOp:
					TRY(parse_bank_field_op(context));
					return dummy_return;
				case AML::ExtOp::DeviceOp:
					TRY(parse_device_op(context));
					return dummy_return;
				case AML::ExtOp::ProcessorOp:
					TRY(parse_processor_op(context));
					return dummy_return;
				case AML::ExtOp::PowerResOp:
					TRY(parse_power_resource_op(context));
					return dummy_return;
				case AML::ExtOp::ThermalZoneOp:
					TRY(parse_thermal_zone_op(context));
					return dummy_return;
				default:
					break;
			}
		}

		switch (static_cast<AML::Byte>(context.aml_data[0]))
		{
			case AML::Byte::AliasOp:
				TRY(parse_alias_op(context));
				return dummy_return;
			case AML::Byte::NameOp:
				TRY(parse_name_op(context));
				return dummy_return;
			case AML::Byte::MethodOp:
				TRY(parse_method_op(context));
				return dummy_return;
			case AML::Byte::NoopOp:
			case AML::Byte::BreakPointOp:
				context.aml_data = context.aml_data.slice(1);
				return dummy_return;
			case AML::Byte::ScopeOp:
				TRY(parse_scope_op(context));
				return dummy_return;
			case AML::Byte::NotifyOp:
				TRY(parse_notify_op(context));
				return dummy_return;
			case AML::Byte::CreateBitFieldOp:
			case AML::Byte::CreateByteFieldOp:
			case AML::Byte::CreateWordFieldOp:
			case AML::Byte::CreateDWordFieldOp:
			case AML::Byte::CreateQWordFieldOp:
				TRY(parse_createfield_op(context));
				return dummy_return;
			case AML::Byte::IfOp:
				return parse_if_op(context);
			case AML::Byte::WhileOp:
				return parse_while_op(context);
			case AML::Byte::BreakOp:
				dprintln_if(AML_DUMP_FUNCTION_CALLS, "parse_break_op");
				context.aml_data = context.aml_data.slice(1);
				return ExecutionFlowResult {
					.elem1 = ExecutionFlow::Break,
					.elem2 = BAN::Optional<Node>(),
				};
			case AML::Byte::ContinueOp:
				dprintln_if(AML_DUMP_FUNCTION_CALLS, "parse_continue_op");
				context.aml_data = context.aml_data.slice(1);
				return ExecutionFlowResult {
					.elem1 = ExecutionFlow::Continue,
					.elem2 = BAN::Optional<Node>(),
				};
			case AML::Byte::ReturnOp:
			{
				dprintln_if(AML_DUMP_FUNCTION_CALLS, "parse_return_op");
				context.aml_data = context.aml_data.slice(1);
				return ExecutionFlowResult {
					.elem1 = ExecutionFlow::Return,
					.elem2 = TRY(parse_node(context)),
				};
			}
			default:
				break;
		}

		auto node = TRY(parse_node(context));
		return ExecutionFlowResult {
			.elem1 = ExecutionFlow::Normal,
			.elem2 = BAN::move(node)
		};
	}

#pragma GCC diagnostic pop

	BAN::ErrorOr<NameString> NameString::from_string(BAN::StringView name)
	{
		NameString result;

		if (name.front() == '\\')
		{
			result.base = base_root;
			name = name.substring(1);
		}
		else
		{
			for (size_t i = 0; i < name.size() && name[i] == '^'; i++)
				result.base++;
			name = name.substring(result.base);
		}

		ASSERT((name.size() % 4) == 0);
		TRY(result.parts.reserve(name.size() / 4));

		while (!name.empty())
		{
			const uint32_t name_seg {
				(static_cast<uint32_t>(name[0]) <<  0) |
				(static_cast<uint32_t>(name[1]) <<  8) |
				(static_cast<uint32_t>(name[2]) << 16) |
				(static_cast<uint32_t>(name[3]) << 24)
			};
			TRY(result.parts.push_back(name_seg));
			name = name.substring(4);
		}

		return result;
	}

	ParseContext::~ParseContext()
	{
		for (auto*& local : locals)
		{
			if (local && --local->ref_count == 0)
				delete local;
			local = nullptr;
		}
		for (auto*& arg : args)
		{
			if (arg && --arg->ref_count == 0)
				delete arg;
			arg = nullptr;
		}
	}

	BAN::ErrorOr<void> ParseContext::allocate_locals()
	{
		for (auto*& local : locals)
		{
			ASSERT(local == nullptr);
			local = new Reference();
			if (local == nullptr)
				return BAN::Error::from_errno(ENOMEM);
			local->ref_count = 1;
		}

		return {};
	}

	BAN::ErrorOr<Node> Node::copy() const
	{
		Node result {};
		result.type = this->type;

		switch (this->type)
		{
			case Type::Uninitialized:
				break;
			case Type::Debug:
				break;
			case Type::Integer:
				result.as.integer.value = this->as.integer.value;
				break;
			case Type::String:
			case Type::Buffer:
				ASSERT(this->as.str_buf);
				result.as.str_buf = static_cast<Buffer*>(kmalloc(sizeof(Buffer) + this->as.str_buf->size));
				if (result.as.str_buf == nullptr)
					return BAN::Error::from_errno(ENOMEM);
				memcpy(result.as.str_buf->bytes, this->as.str_buf->bytes, this->as.str_buf->size);
				result.as.str_buf->size = this->as.str_buf->size;
				result.as.str_buf->ref_count = 1;
				break;
			case Type::Package:
				ASSERT(this->as.package);
				result.as.package = static_cast<Package*>(kmalloc(sizeof(Package) + this->as.package->num_elements * sizeof(Package::Element)));
				if (result.as.package == nullptr)
					return BAN::Error::from_errno(ENOMEM);
				result.as.package->num_elements = this->as.package->num_elements;
				result.as.package->ref_count = 1;
				for (size_t i = 0; i < result.as.package->num_elements; i++)
				{
					auto&       dst_elem = result.as.package->elements[i];
					const auto& src_elem =  this->as.package->elements[i];

					if (src_elem.resolved)
					{
						dst_elem.resolved = true;
						dst_elem.value.node = nullptr;
						if (src_elem.value.node)
						{
							dst_elem.value.node = new Node();
							if (dst_elem.value.node == nullptr)
								return BAN::Error::from_errno(ENOMEM);
							*dst_elem.value.node = TRY(src_elem.value.node->copy());
						}
					}
					else
					{
						dst_elem.resolved = false;
						dst_elem.value.location = nullptr;
						if (src_elem.value.location)
						{
							dst_elem.value.location = new Package::Element::Location();
							if (dst_elem.value.location == nullptr)
								return BAN::Error::from_errno(ENOMEM);
							dst_elem.value.location->name = TRY(src_elem.value.location->name.copy());
							dst_elem.value.location->scope = TRY(src_elem.value.location->scope.copy());
						}
					}
				}
				break;
			case Type::BufferField:
				result.as.buffer_field = this->as.buffer_field;
				result.as.buffer_field.buffer->ref_count++;
				break;
			case Type::OpRegion:
				result.as.opregion = this->as.opregion;
				break;
			case Type::FieldUnit:
				result.as.field_unit = this->as.field_unit;
				switch (result.as.field_unit.type)
				{
					case FieldUnit::Type::Field:
						break;
					case FieldUnit::Type::IndexField:
						result.as.field_unit.as.index_field.index->ref_count++;
						result.as.field_unit.as.index_field.data->ref_count++;
						break;
					case FieldUnit::Type::BankField:
						result.as.field_unit.as.bank_field.bank_selector->ref_count++;
						break;
					default:
						ASSERT_NOT_REACHED();
				}
				break;
			case Type::Method:
				result.as.method = this->as.method;
				if (result.as.method.mutex)
					result.as.method.mutex->ref_count++;
				break;
			case Type::Index:
				switch (this->as.index.type)
				{
					case Node::Type::String:
					case Node::Type::Buffer:
						ASSERT(this->as.index.as.str_buf);
						result.as.index.as.str_buf = this->as.index.as.str_buf;
						result.as.index.as.str_buf->ref_count++;
						break;
					case Node::Type::Package:
						ASSERT(this->as.index.as.package);
						result.as.index.as.package = this->as.index.as.package;
						result.as.index.as.package->ref_count++;
						break;
					default: ASSERT_NOT_REACHED();
				}
				result.as.index.type = this->as.index.type;
				result.as.index.index = this->as.index.index;
				break;
			case Type::Reference:
				ASSERT(this->as.reference);
				result.as.reference = this->as.reference;
				result.as.reference->ref_count++;
				break;
			case Type::Event:
				dwarnln("Copy Event");
				return BAN::Error::from_errno(EINVAL);
			case Type::Device:
				dwarnln("Copy Device");
				return BAN::Error::from_errno(EINVAL);
			case Type::Processor:
				dwarnln("Copy Processor");
				return BAN::Error::from_errno(EINVAL);
			case Type::PowerResource:
				dwarnln("Copy PowerResource");
				return BAN::Error::from_errno(EINVAL);
			case Type::ThermalZone:
				dwarnln("Copy ThremalZone");
				return BAN::Error::from_errno(EINVAL);
			case Type::Mutex:
				dwarnln("Copy Mutex");
				return BAN::Error::from_errno(EINVAL);
			case Type::PredefinedScope:
				dwarnln("Copy Scope");
				return BAN::Error::from_errno(EINVAL);
			case Type::Count:
				ASSERT_NOT_REACHED();
		}

		return result;
	}

	Node& Node::operator=(Node&& other)
	{
		clear();

		switch (other.type)
		{
			case Type::Uninitialized:
				break;
			case Type::Debug:
				break;
			case Type::Integer:
				this->as.integer = other.as.integer;
				other.as.integer = {};
				break;
			case Type::String:
			case Type::Buffer:
				this->as.str_buf = other.as.str_buf;
				other.as.str_buf = {};
				break;
			case Type::Package:
				this->as.package = other.as.package;
				other.as.package = {};
				break;
			case Type::BufferField:
				this->as.buffer_field = other.as.buffer_field;
				other.as.buffer_field = {};
				break;
			case Type::OpRegion:
				this->as.opregion = other.as.opregion;
				other.as.opregion = {};
				break;
			case Type::FieldUnit:
				this->as.field_unit = other.as.field_unit;
				other.as.field_unit = {};
				break;
			case Type::Event:
				break;
			case Type::Device:
				break;
			case Type::Processor:
				break;
			case Type::PowerResource:
				break;
			case Type::ThermalZone:
				break;
			case Type::Method:
				this->as.method = other.as.method;
				other.as.method = {};
				break;
			case Type::Mutex:
				this->as.mutex = other.as.mutex;
				other.as.mutex = {};
				break;
			case Type::Index:
				this->as.index = other.as.index;
				other.as.index = {};
				break;
			case Type::Reference:
				this->as.reference = other.as.reference;
				other.as.reference = {};
				break;
			case Type::PredefinedScope:
				break;
			case Type::Count:
				ASSERT_NOT_REACHED();
		}
		this->type = other.type;
		other.type = Node::Type::Uninitialized;

		return *this;
	}

	static void deref_package(Package* package)
	{
		if (package == nullptr || --package->ref_count != 0)
			return;
		for (size_t i = 0; i < package->num_elements; i++)
		{
			auto& elem = package->elements[i];

			if (elem.resolved)
			{
				if (elem.value.node)
					delete elem.value.node;
				elem.value.node = nullptr;
			}
			else
			{
				if (elem.value.location)
					delete elem.value.location;
				elem.value.location = nullptr;
			}
		}
		kfree(package);
	}

	void Node::clear()
	{
		switch (this->type)
		{
			case Type::Uninitialized:
				break;
			case Type::Debug:
				break;
			case Type::Integer:
				break;
			case Type::String:
			case Type::Buffer:
				if (this->as.str_buf && --this->as.str_buf->ref_count == 0)
					kfree(this->as.str_buf);
				this->as.str_buf = {};
				break;
			case Type::Package:
				deref_package(this->as.package);
				this->as.package = {};
				break;
			case Type::BufferField:
				if (this->as.buffer_field.buffer && --this->as.buffer_field.buffer->ref_count == 0)
					delete this->as.buffer_field.buffer;
				this->as.buffer_field = {};
				break;
			case Type::OpRegion:
				this->as.opregion = {};
				break;
			case Type::FieldUnit:
				switch (this->as.field_unit.type)
				{
					case FieldUnit::Type::Field:
						break;
					case FieldUnit::Type::IndexField:
						if (--this->as.field_unit.as.index_field.index->ref_count == 0)
							delete this->as.field_unit.as.index_field.index;
						if (--this->as.field_unit.as.index_field.data->ref_count == 0)
							delete this->as.field_unit.as.index_field.data;
						break;
					case FieldUnit::Type::BankField:
						if (--this->as.field_unit.as.bank_field.bank_selector->ref_count == 0)
							delete this->as.field_unit.as.bank_field.bank_selector;
						break;
					default:
						ASSERT_NOT_REACHED();
				}
				this->as.field_unit = {};
				break;
			case Type::Event:
				break;
			case Type::Device:
				break;
			case Type::Processor:
				break;
			case Type::PowerResource:
				break;
			case Type::ThermalZone:
				break;
			case Type::Method:
				if (this->as.method.mutex && --this->as.method.mutex->ref_count == 0)
					delete this->as.method.mutex;
				this->as.method = {};
				break;
			case Type::Mutex:
				if (this->as.mutex && --this->as.mutex->ref_count == 0)
					delete this->as.mutex;
				this->as.mutex = {};
				break;
			case Type::Index:
				switch (this->as.index.type)
				{
					case Type::Uninitialized:
						break;
					case Type::String:
					case Type::Buffer:
						if (this->as.index.as.str_buf && --this->as.index.as.str_buf->ref_count == 0)
							kfree(this->as.index.as.str_buf);
						break;
					case Type::Package:
						deref_package(this->as.index.as.package);
						break;
					default: ASSERT_NOT_REACHED();
				}
				this->as.index = {};
				break;
			case Type::Reference:
				if (this->as.reference && --this->as.reference->ref_count == 0)
					delete this->as.reference;
				this->as.reference = {};
				break;
			case Type::PredefinedScope:
				break;
			case Type::Count:
				ASSERT_NOT_REACHED();
		}
		this->type = Type::Uninitialized;
	}

}
