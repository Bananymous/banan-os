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
	};

	struct FieldElement : public NamedObject
	{
		uint64_t bit_offset;
		uint32_t bit_count;

		FieldRules access_rules;

		BAN::RefPtr<OpRegion> op_region = nullptr;

		FieldElement(NameSeg name, uint64_t bit_offset, uint32_t bit_count, FieldRules access_rules)
			: NamedObject(Node::Type::FieldElement, name)
			, bit_offset(bit_offset)
			, bit_count(bit_count)
			, access_rules(access_rules)
		{}

		BAN::RefPtr<Node> evaluate() override;
		bool store(BAN::RefPtr<Node> source) override;

		void debug_print(int indent) const override;

	private:
		struct AccessType
		{
			uint64_t offset;
			uint64_t mask;
			uint32_t access_size;
			uint32_t shift;
		};
		BAN::Optional<AccessType> determine_access_type() const;
		BAN::Optional<uint64_t> read_field(uint64_t offset, uint32_t access_size) const;
		bool write_field(uint64_t offset, uint32_t access_size, uint64_t value) const;
	};

	struct Field
	{
		static ParseResult parse(ParseContext& context);
	};

	struct IndexFieldElement : public NamedObject
	{
		uint64_t bit_offset;
		uint32_t bit_count;

		FieldRules access_rules;

		BAN::RefPtr<FieldElement> index_element = nullptr;
		BAN::RefPtr<FieldElement> data_element = nullptr;

		IndexFieldElement(NameSeg name, uint64_t bit_offset, uint32_t bit_count, FieldRules access_rules)
			: NamedObject(Node::Type::IndexFieldElement, name)
			, bit_offset(bit_offset)
			, bit_count(bit_count)
			, access_rules(access_rules)
		{}

		void debug_print(int indent) const override;
	};

	struct IndexField
	{
		static ParseResult parse(ParseContext& context);
	};

}
