#include <kernel/ACPI/AML/Buffer.h>
#include <kernel/ACPI/AML/String.h>

namespace Kernel::ACPI::AML
{

	BAN::Optional<bool> String::logical_compare(BAN::RefPtr<AML::Node> node, AML::Byte binaryop)
	{
		auto rhs = node ? node->convert(AML::Node::ConvString) : BAN::RefPtr<AML::Node>();
		if (!rhs)
		{
			AML_ERROR("String logical compare RHS is not string");
			return {};
		}

		(void)binaryop;
		AML_TODO("Logical compare string");
		return {};
	}

	BAN::RefPtr<AML::Buffer> String::as_buffer()
	{
		auto buffer = MUST(BAN::RefPtr<AML::Buffer>::create());
		MUST(buffer->buffer.resize(string.size()));
		for (size_t i = 0; i < string.size(); i++)
			buffer->buffer[i] = string[i];
		return buffer;
	}

	BAN::RefPtr<AML::Node> String::convert(uint8_t mask)
	{
		if (mask & AML::Node::ConvString)
			return this;
		if (mask & AML::Node::ConvInteger)
		{
			AML_TODO("Convert String to Integer");
			return {};
		}
		if (mask & AML::Node::ConvBuffer)
			return as_buffer();
		return {};
	}

	ParseResult String::parse(ParseContext& context)
	{
		ASSERT(context.aml_data.size() >= 1);
		ASSERT(static_cast<AML::Byte>(context.aml_data[0]) == AML::Byte::StringPrefix);
		context.aml_data = context.aml_data.slice(1);

		BAN::Vector<uint8_t> string;

		while (context.aml_data.size() > 0)
		{
			if (context.aml_data[0] == 0x00)
				break;
			MUST(string.push_back(context.aml_data[0]));
			context.aml_data = context.aml_data.slice(1);
		}

		if (context.aml_data.size() == 0)
			return ParseResult::Failure;
		if (context.aml_data[0] != 0x00)
			return ParseResult::Failure;
		context.aml_data = context.aml_data.slice(1);

		auto string_node = MUST(BAN::RefPtr<String>::create());
		string_node->string = BAN::move(string);

		return ParseResult(string_node);
	}

}
