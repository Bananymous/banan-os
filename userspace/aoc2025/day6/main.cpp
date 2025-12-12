#include <BAN/String.h>
#include <BAN/Vector.h>

#include <ctype.h>
#include <inttypes.h>
#include <stdio.h>

using i8  = int8_t;
using i16 = int16_t;
using i32 = int32_t;
using i64 = int64_t;

using u8  = uint8_t;
using u16 = uint16_t;
using u32 = uint32_t;
using u64 = uint64_t;

i64 part1(FILE* fp)
{
	struct Operation
	{
		BAN::Vector<i64> values;
		char op;
	};

	BAN::Vector<Operation> operations;

	BAN::String line;
	char line_buffer[128];
	while (fgets(line_buffer, sizeof(line_buffer), fp))
	{
		MUST(line.append(line_buffer));
		if (line.back() != '\n')
			continue;
		line.pop_back();

		auto parts = MUST(line.sv().split(' '));
		if (operations.size() < parts.size())
			MUST(operations.resize(parts.size()));

		if (isdigit(parts[0][0]))
		{
			for (size_t i = 0; i < parts.size(); i++)
				MUST(operations[i].values.push_back(atoll(parts[i].data())));
			line.clear();
		}
		else
		{
			for (size_t i = 0; i < parts.size(); i++)
				operations[i].op = parts[i][0];
			break;
		}
	}

	i64 result = 0;

	for (const auto& operation : operations)
	{
		i64 value;
		switch (operation.op)
		{
			case '+':
				value = 0;
				for (auto val : operation.values)
					value += val;
				break;
			case '*':
				value = 1;
				for (auto val : operation.values)
					value *= val;
				break;
			default:
				ASSERT_NOT_REACHED();
		}
		result += value;
	}

	return result;
}

i64 part2(FILE* fp)
{
	BAN::Vector<BAN::String> lines;
	MUST(lines.emplace_back());

	char line_buffer[128];
	while (fgets(line_buffer, sizeof(line_buffer), fp))
	{
		MUST(lines.back().append(line_buffer));
		if (lines.back().back() != '\n')
			continue;
		lines.back().pop_back();
		MUST(lines.emplace_back());
	}

	while (lines.back().empty())
		lines.pop_back();

	i64 result = 0;

	char op = 0;
	i64 current = 0;
	for (size_t i = 0; i < lines.front().size(); i++)
	{
		if (lines.back()[i] != ' ')
		{
			result += current;

			op = lines.back()[i];
			current = (op == '+') ? 0 : 1;
		}

		i64 value = 0;
		bool all_space = true;
		for (size_t j = 0; j < lines.size() - 1; j++)
		{
			if (isdigit(lines[j][i]))
				value = (value * 10) + (lines[j][i] - '0');
			all_space = all_space && lines[j][i] == ' ';
		}

		if (all_space)
			continue;

		current = (op == '+') ? current + value : current * value;
	}

	return result + current;
}

int main(int argc, char** argv)
{
	const char* file_path = "/usr/share/aoc2025/day6_input.txt";

	if (argc >= 2)
		file_path = argv[1];

	FILE* fp = fopen(file_path, "r");
	if (fp == nullptr)
	{
		perror("fopen");
		return 1;
	}

	printf("part1: %" PRId64 "\n", part1(fp));

	fseek(fp, 0, SEEK_SET);

	printf("part2: %" PRId64 "\n", part2(fp));

	fclose(fp);
}
