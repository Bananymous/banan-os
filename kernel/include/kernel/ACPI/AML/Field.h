#pragma once

#include <kernel/ACPI/AML/NamedObject.h>
#include <kernel/ACPI/AML/ParseContext.h>
#include <kernel/ACPI/AML/Pkg.h>
#include <kernel/ACPI/AML/Region.h>

namespace Kernel::ACPI::AML
{

	struct FieldRules
	{
		enum class AccessType
		{
			Any = 0,
			Byte = 1,
			Word = 2,
			DWord = 3,
			QWord = 4,
			Buffer = 5,
		};
		AccessType access_type;

		enum class LockRule
		{
			NoLock = 0,
			Lock = 1,
		};
		LockRule lock_rule;

		enum class UpdateRule
		{
			Preserve = 0,
			WriteAsOnes = 1,
			WriteAsZeros = 2,
		};
		UpdateRule update_rule;

		enum class AccessAttrib
		{
			Normal = 0,
			Bytes = 1,
			RawBytes = 2,
			RawProcessBytes = 3,
		};
		AccessAttrib access_attrib = AccessAttrib::Normal;
		uint8_t access_length = 0;
	};

	struct FieldElement : public NamedObject
	{
		uint64_t bit_offset;
		uint64_t bit_count;

		FieldRules access_rules;

		BAN::RefPtr<OpRegion> op_region;

		FieldElement(NameSeg name, uint64_t bit_offset, uint64_t bit_count, FieldRules access_rules)
			: NamedObject(Node::Type::FieldElement, name)
			, bit_offset(bit_offset)
			, bit_count(bit_count)
			, access_rules(access_rules)
		{}

		BAN::RefPtr<Node> evaluate() override;
		bool store(BAN::RefPtr<Node> source) override;

		void debug_print(int indent) const override;

	private:
		BAN::Optional<uint64_t> evaluate_internal();
		bool store_internal(uint64_t value);

		friend struct IndexFieldElement;
		friend struct BankFieldElement;
	};

	struct Field
	{
		static ParseResult parse(ParseContext& context);
	};

	struct IndexFieldElement : public NamedObject
	{
		uint64_t bit_offset;
		uint64_t bit_count;

		FieldRules access_rules;

		BAN::RefPtr<FieldElement> index_element;
		BAN::RefPtr<FieldElement> data_element;

		IndexFieldElement(NameSeg name, uint64_t bit_offset, uint64_t bit_count, FieldRules access_rules)
			: NamedObject(Node::Type::IndexFieldElement, name)
			, bit_offset(bit_offset)
			, bit_count(bit_count)
			, access_rules(access_rules)
		{}

		BAN::RefPtr<Node> evaluate() override;
		bool store(BAN::RefPtr<Node> source) override;

		void debug_print(int indent) const override;
	};

	struct IndexField
	{
		static ParseResult parse(ParseContext& context);
	};

	struct BankFieldElement : public NamedObject
	{
		uint64_t bit_offset;
		uint64_t bit_count;

		FieldRules access_rules;

		BAN::RefPtr<OpRegion> op_region;
		BAN::RefPtr<FieldElement> bank_selector;
		uint64_t bank_value;

		BankFieldElement(NameSeg name, uint64_t bit_offset, uint64_t bit_count, FieldRules access_rules)
			: NamedObject(Node::Type::BankFieldElement, name)
			, bit_offset(bit_offset)
			, bit_count(bit_count)
			, access_rules(access_rules)
		{}

		BAN::RefPtr<Node> evaluate() override;
		bool store(BAN::RefPtr<Node> source) override;

		void debug_print(int indent) const override;
	};

	struct BankField
	{
		static ParseResult parse(ParseContext& context);
	};

}
