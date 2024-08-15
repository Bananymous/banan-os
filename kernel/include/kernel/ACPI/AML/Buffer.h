#pragma once

#include <kernel/ACPI/AML/Bytes.h>
#include <kernel/ACPI/AML/Integer.h>
#include <kernel/ACPI/AML/Node.h>
#include <kernel/ACPI/AML/ParseContext.h>
#include <kernel/ACPI/AML/Pkg.h>
#include <kernel/ACPI/AML/String.h>

namespace Kernel::ACPI::AML
{

	struct Buffer final : public AML::Node
	{
		BAN::Vector<uint8_t> buffer;

		Buffer()
			: AML::Node(Node::Type::Buffer)
		{}

		BAN::Optional<bool> logical_compare(BAN::RefPtr<AML::Node> node, AML::Byte binaryop)
		{
			auto rhs_node = node ? node->convert(AML::Node::ConvBuffer) : BAN::RefPtr<AML::Node>();
			if (!rhs_node)
			{
				AML_ERROR("Buffer logical compare RHS cannot be converted to buffer");
				return {};
			}

			(void)binaryop;
			AML_TODO("Logical compare buffer");
			return {};
		}

		BAN::RefPtr<AML::Node> convert(uint8_t mask) override
		{
			if (mask & AML::Node::ConvBuffer)
				return this;
			if (mask & AML::Node::ConvInteger)
				return as_integer();
			if (mask & AML::Node::ConvString)
			{
				AML_TODO("Convert BufferField to String");
				return {};
			}
			return {};
		}

		BAN::RefPtr<AML::Node> store(BAN::RefPtr<AML::Node> node) override
		{
			ASSERT(node);
			auto conv_node = node->convert(AML::Node::ConvBuffer);
			if (!conv_node)
			{
				AML_ERROR("Buffer store could not convert to buffer");
				return {};
			}

			auto& conv_buffer = static_cast<AML::Buffer*>(conv_node.ptr())->buffer;
			MUST(buffer.resize(conv_buffer.size()));
			for (size_t i = 0; i < buffer.size(); i++)
				buffer[i] = conv_buffer[i];
			return this;
		}

		static ParseResult parse(AML::ParseContext& context)
		{
			ASSERT(context.aml_data.size() >= 1);
			ASSERT(static_cast<Byte>(context.aml_data[0]) == Byte::BufferOp);
			context.aml_data = context.aml_data.slice(1);

			auto buffer_pkg = AML::parse_pkg(context.aml_data);
			if (!buffer_pkg.has_value())
				return ParseResult::Failure;

			auto buffer_context = context;
			buffer_context.aml_data = buffer_pkg.value();

			auto buffer_size_result = AML::parse_object(buffer_context);
			if (!buffer_size_result.success())
				return ParseResult::Failure;

			auto buffer_size_node = buffer_size_result.node() ? buffer_size_result.node()->convert(AML::Node::ConvInteger) : BAN::RefPtr<AML::Node>();
			if (!buffer_size_node)
			{
				AML_ERROR("Buffer size is not an integer");
				return ParseResult::Failure;
			}

			const uint32_t actual_buffer_size = BAN::Math::max<uint32_t>(
				static_cast<AML::Integer*>(buffer_size_node.ptr())->value,
				buffer_context.aml_data.size()
			);

			auto buffer = MUST(BAN::RefPtr<Buffer>::create());
			MUST(buffer->buffer.resize(actual_buffer_size, 0));
			for (uint32_t i = 0; i < buffer_context.aml_data.size(); i++)
					buffer->buffer[i] = buffer_context.aml_data[i];

#if AML_DEBUG_LEVEL >= 2
			buffer->debug_print(0);
			AML_DEBUG_PRINTLN("");
#endif

			return ParseResult(buffer);
		}

		virtual void debug_print(int indent) const override
		{
			AML_DEBUG_PRINT_INDENT(indent);
			AML_DEBUG_PRINT("Buffer ({} bytes)", buffer.size());
		}

	private:
		BAN::RefPtr<AML::Integer> as_integer()
		{
			uint64_t value = 0;
			for (size_t i = 0; i < BAN::Math::min<size_t>(buffer.size(), 8); i++)
				value |= static_cast<uint64_t>(buffer[i]) << (8 * i);
			return MUST(BAN::RefPtr<Integer>::create(value));
		}
	};

	struct BufferField final : AML::NamedObject
	{
		BAN::RefPtr<AML::Node> buffer;
		size_t field_bit_offset;
		size_t field_bit_size;

		template<typename T> requires BAN::is_same_v<T, AML::Buffer> || BAN::is_same_v<T, AML::String>
		BufferField(AML::NameSeg name, BAN::RefPtr<T> buffer, size_t field_bit_offset, size_t field_bit_size)
			: AML::NamedObject(Node::Type::BufferField, name)
			, buffer(buffer)
			, field_bit_offset(field_bit_offset)
			, field_bit_size(field_bit_size)
		{}

		BAN::RefPtr<AML::Node> convert(uint8_t mask) override
		{
			if (mask & AML::Node::ConvBufferField)
				return this;
			if (mask & AML::Node::ConvInteger)
				return as_integer();
			if (mask & AML::Node::ConvBuffer)
			{
				AML_TODO("Convert BufferField to Buffer");
				return {};
			}
			if (mask & AML::Node::ConvString)
			{
				AML_TODO("Convert BufferField to String");
				return {};
			}
			return {};
		}

		BAN::RefPtr<AML::Node> store(BAN::RefPtr<AML::Node> node) override
		{
			ASSERT(buffer);
			ASSERT(buffer->type == AML::Node::Type::Buffer || buffer->type == AML::Node::Type::String);
			auto& buffer = (this->buffer->type == AML::Node::Type::Buffer)
				? static_cast<AML::Buffer*>(this->buffer.ptr())->buffer
				: static_cast<AML::String*>(this->buffer.ptr())->string;
			ASSERT(field_bit_offset + field_bit_size <= buffer.size() * 8);

			auto value_node = node ? node->convert(AML::Node::ConvInteger) : BAN::RefPtr<AML::Node>();
			if (!value_node)
				return {};
			const auto value = static_cast<AML::Integer*>(value_node.ptr())->value;

			// TODO: optimize for whole byte accesses
			for (size_t i = 0; i < field_bit_size; i++)
			{
				const size_t bit = field_bit_offset + i;
				buffer[bit / 8] &= ~(1 << (bit % 8));
				buffer[bit / 8] |= ((value >> i) & 1) << (bit % 8);
			}

			return value_node;
		}

		static ParseResult parse(AML::ParseContext& context)
		{
			ASSERT(context.aml_data.size() >= 1);

			bool offset_in_bytes;
			size_t field_bit_size;
			switch (static_cast<AML::Byte>(context.aml_data[0]))
			{
				case AML::Byte::CreateBitFieldOp:
					field_bit_size = 1;
					offset_in_bytes = false;
					break;
				case AML::Byte::CreateByteFieldOp:
					field_bit_size = 8;
					offset_in_bytes = true;
					break;
				case AML::Byte::CreateWordFieldOp:
					field_bit_size = 16;
					offset_in_bytes = true;
					break;
				case AML::Byte::CreateDWordFieldOp:
					field_bit_size = 32;
					offset_in_bytes = true;
					break;
				case AML::Byte::CreateQWordFieldOp:
					field_bit_size = 64;
					offset_in_bytes = true;
					break;
				case AML::Byte::ExtOpPrefix:
					ASSERT(context.aml_data.size() >= 2);
					ASSERT(static_cast<AML::ExtOp>(context.aml_data[1]) == AML::ExtOp::CreateFieldOp);
					field_bit_size = 0;
					offset_in_bytes = false;
					break;
				default:
					ASSERT_NOT_REACHED();
			}
			context.aml_data = context.aml_data.slice(1 + (static_cast<AML::Byte>(context.aml_data[0]) == AML::Byte::ExtOpPrefix));

			auto buffer_result = AML::parse_object(context);
			if (!buffer_result.success())
				return ParseResult::Failure;
			auto buffer_node = buffer_result.node() ? buffer_result.node()->convert(AML::Node::ConvBuffer) : BAN::RefPtr<AML::Node>();
			if (!buffer_node || buffer_node->type != Node::Type::Buffer)
			{
				AML_ERROR("Buffer source does not evaluate to a Buffer");
				return ParseResult::Failure;
			}
			auto buffer = BAN::RefPtr<AML::Buffer>(static_cast<AML::Buffer*>(buffer_node.ptr()));

			auto index_result = AML::parse_object(context);
			if (!index_result.success())
				return ParseResult::Failure;
			auto index_node = index_result.node() ? index_result.node()->convert(AML::Node::ConvInteger) : BAN::RefPtr<AML::Node>();
			if (!index_node)
			{
				AML_ERROR("Failed to parse index for BufferField");
				return ParseResult::Failure;
			}

			if (field_bit_size == 0)
			{
				auto bit_count_result = AML::parse_object(context);
				if (!bit_count_result.success())
					return ParseResult::Failure;
				auto bit_count_node = bit_count_result.node() ? bit_count_result.node()->convert(AML::Node::ConvInteger) : BAN::RefPtr<AML::Node>();
				if (!bit_count_node)
				{
					AML_ERROR("Failed to parse bit count for BufferField");
					return ParseResult::Failure;
				}
				field_bit_size = static_cast<AML::Integer*>(bit_count_node.ptr())->value;
			}

			auto field_name = AML::NameString::parse(context.aml_data);
			if (!field_name.has_value())
				return ParseResult::Failure;
			if (field_name->path.empty())
			{
				AML_ERROR("Empty field name for BufferField");
				return ParseResult::Failure;
			}

			size_t field_bit_offset = static_cast<AML::Integer*>(index_node.ptr())->value;
			if (offset_in_bytes)
				field_bit_offset *= 8;

			auto field = MUST(BAN::RefPtr<BufferField>::create(field_name->path.back(), buffer, field_bit_offset, field_bit_size));
			if (!Namespace::root_namespace()->add_named_object(context, field_name.value(), field))
				return ParseResult::Success;

#if AML_DEBUG_LEVEL >= 2
			field->debug_print(0);
			AML_DEBUG_PRINTLN("");
#endif

			return ParseResult::Success;
		}

		virtual void debug_print(int indent) const override
		{
			AML_DEBUG_PRINT_INDENT(indent);
			AML_DEBUG_PRINT("BufferField {} at bit offset {} ({} bits) to { ", name, field_bit_offset, field_bit_size);
			buffer->debug_print(0);
			AML_DEBUG_PRINT(" }");
		}

	private:
		BAN::RefPtr<AML::Integer> as_integer()
		{
			ASSERT(buffer);
			ASSERT(buffer->type == AML::Node::Type::Buffer || buffer->type == AML::Node::Type::String);

			const auto& buffer = (this->buffer->type == AML::Node::Type::Buffer)
				? static_cast<AML::Buffer*>(this->buffer.ptr())->buffer
				: static_cast<AML::String*>(this->buffer.ptr())->string;

			uint64_t value = 0;

			// TODO: optimize for whole byte accesses
			for (size_t i = 0; i < BAN::Math::min<size_t>(field_bit_size, 64); i++)
			{
				const size_t bit = field_bit_offset + i;
				value |= static_cast<uint64_t>((buffer[bit / 8] >> (bit % 8)) & 1) << i;
			}

			return MUST(BAN::RefPtr<Integer>::create(value));
		}
	};

}
