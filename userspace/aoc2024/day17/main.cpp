#include <BAN/Vector.h>
#include <BAN/String.h>

#include <ctype.h>
#include <inttypes.h>
#include <stdio.h>

using i8  = int8_t;
using i16 = int16_t;
using i32 = int32_t;
using i64 = int64_t;
using isize = ssize_t;

using u8  = uint8_t;
using u16 = uint16_t;
using u32 = uint32_t;
using u64 = uint64_t;
using usize = size_t;

struct Registers
{
	u64 a, b, c;
	usize ip;
};

enum Opcodes
{
	ADV = 0,
	BXL = 1,
	BST = 2,
	JNZ = 3,
	BXC = 4,
	OUT = 5,
	BDV = 6,
	CDV = 7,
};

using Program = BAN::Vector<u8>;

struct ParseInputResult
{
	Registers registers;
	Program program;
};

static ParseInputResult parse_input(FILE* fp)
{
	Registers registers;
	ASSERT(fscanf(fp,
		"Register A: %" SCNu64 "\n"
		"Register B: %" SCNu64 "\n"
		"Register C: %" SCNu64 "\n"
		"\n"
		"Program: ",
		&registers.a, &registers.b, &registers.c
	) == 3);
	registers.ip = 0;

	char buffer[128];
	ASSERT(fgets(buffer, sizeof(buffer), fp));

	Program program;
	for (usize i = 0; buffer[i]; i++)
		if (isdigit(buffer[i]))
			MUST(program.push_back(buffer[i] - '0'));

	return ParseInputResult {
		.registers = BAN::move(registers),
		.program = BAN::move(program)
	};
}

BAN::Vector<u64> emulate_program(Registers registers, const Program& program)
{
	const auto combo =
		[&registers](u8 combo) -> u64
		{
			switch (combo)
			{
				case 0: case 1: case 2: case 3: return combo;
				case 4: return registers.a;
				case 5: return registers.b;
				case 6: return registers.c;
			}
			ASSERT_NOT_REACHED();
		};

	BAN::Vector<u64> output;
	while (registers.ip < program.size())
	{
		const u8 opcode  = program[registers.ip + 0];
		const u8 operand = program[registers.ip + 1];
		registers.ip += 2;

		switch (opcode)
		{
			case Opcodes::ADV:
				registers.a = registers.a >> combo(operand);
				break;
			case Opcodes::BXL:
				registers.b = registers.b ^ operand;
				break;
			case Opcodes::BST:
				registers.b = combo(operand) & 0x07;
				break;
			case Opcodes::JNZ:
				if (registers.a != 0)
					registers.ip = operand;
				break;
			case Opcodes::BXC:
				registers.b = registers.b ^ registers.c;
				break;
			case Opcodes::OUT:
				MUST(output.push_back(combo(operand) & 0x07));
				break;
			case Opcodes::BDV:
				registers.b = registers.a >> combo(operand);
				break;
			case Opcodes::CDV:
				registers.c = registers.a >> combo(operand);
				break;
		}
	}

	return output;
}

BAN::String part1(FILE* fp)
{
	auto [registers, program] = parse_input(fp);
	auto output = emulate_program(registers, program);

	BAN::String result;
	MUST(result.resize(output.size() * 2 - 1));

	for (usize i = 0; i < result.size(); i++)
		result[i] = (i % 2) ? ',' : output[i / 2] + '0';
	return result;
}

static BAN::Optional<u64> recurse_part2(Registers initial_registers, u64 curr_a, usize curr_bits, usize output_done, const Program& program)
{
	if (output_done >= program.size())
		return {};

	BAN::Optional<u64> result;
	for (u64 val = 0; val < 8; val++)
	{
		const u64 next_a = curr_a | (val << curr_bits);

		auto registers = initial_registers;
		registers.a = next_a;

		auto output = emulate_program(registers, program);
		if (output.size() < output_done + 1)
			continue;
		if (output.size() > program.size())
			continue;

		bool match = true;
		for (usize i = 0; i < output_done + 1 && match; i++)
			if (output[i] != program[i])
				match = false;
		if (!match)
			continue;

		if (output_done + 1 == program.size())
			return next_a;

		auto temp = recurse_part2(registers, next_a, curr_bits + 3, output_done + 1, program);
		if (temp.has_value())
			result = BAN::Math::min(result.value_or(BAN::numeric_limits<u64>::max()), temp.value());
	}

	return result;
}

i64 part2(FILE* fp)
{
	auto [initial_registers, program] = parse_input(fp);

	BAN::Optional<u64> result;
	for (u64 val = 0; val < 1024; val++)
	{
		auto registers = initial_registers;
		registers.a = val;

		auto output = emulate_program(registers, program);
		if (output.empty() || output.size() > program.size())
			continue;
		if (output[0] != program[0])
			continue;

		auto temp = recurse_part2(registers, val, 10, 1, program);
		if (temp.has_value())
			result = BAN::Math::min(result.value_or(BAN::numeric_limits<u64>::max()), temp.value());
	}

	return result.value();
}

int main(int argc, char** argv)
{
	const char* file_path = "/usr/share/aoc2024/day17_input.txt";

	if (argc >= 2)
		file_path = argv[1];

	FILE* fp = fopen(file_path, "r");
	if (fp == nullptr)
	{
		perror("fopen");
		return 1;
	}

	printf("part1: %s\n", part1(fp).data());

	fseek(fp, 0, SEEK_SET);

	printf("part2: %" PRId64 "\n", part2(fp));

	fclose(fp);
}
