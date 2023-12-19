#include <BAN/Array.h>
#include <BAN/LinkedList.h>
#include <BAN/String.h>
#include <BAN/StringView.h>
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

i64 parse_i64(BAN::StringView string)
{
	i64 result = 0;
	for (char c : string)
		result = (result * 10) + (c - '0');
	return result;
}

BAN::Vector<BAN::StringView> parse_instructions(FILE* fp)
{
	static BAN::String line;

	char buffer[128];
	while (fgets(buffer, sizeof(buffer), fp))
	{
		BAN::StringView buffer_sv(buffer);
		MUST(line.append(buffer_sv));
		if (buffer_sv.back() == '\n')
		{
			line.pop_back();
			break;
		}
	}

	return MUST(line.sv().split(','));
}

i64 calculate_hash(BAN::StringView string)
{
	i64 result = 0;
	for (char c : string)
		result = ((result + c) * 17) % 256;
	return result;
}

i64 puzzle1(FILE* fp)
{
	auto instructions = parse_instructions(fp);

	i64 result = 0;
	for (auto insn : instructions)
		result += calculate_hash(insn);
	return result;
}

i64 puzzle2(FILE* fp)
{
	struct Entry
	{
		BAN::String label;
		i64 focal_length;
	};
	using Box = BAN::LinkedList<Entry>;

	auto instructions = parse_instructions(fp);

	BAN::Array<Box, 256> boxes;

	for (auto insn : instructions)
	{
		if (insn.back() == '-')
		{
			auto label = insn.substring(0, insn.size() - 1);
			i64 hash = calculate_hash(label);

			for (auto it = boxes[hash].begin(); it != boxes[hash].end(); it++)
			{
				if (it->label == label)
				{
					boxes[hash].remove(it);
					break;
				}
			}
		}
		else
		{
			auto temp = MUST(insn.split('='));

			auto label = temp[0];
			auto focal_length = parse_i64(temp[1]);
			i64 hash = calculate_hash(label);

			bool found = false;
			for (auto it = boxes[hash].begin(); it != boxes[hash].end(); it++)
			{
				if (it->label == label)
				{
					it->focal_length = focal_length;
					found = true;
					break;
				}
			}
			if (!found)
				MUST(boxes[hash].emplace_back(label, focal_length));
		}
	}

	i64 result = 0;
	for (size_t i = 0; i < boxes.size(); i++)
	{
		size_t slot = 0;
		for (auto it = boxes[i].begin(); it != boxes[i].end(); it++, slot++)
			result += (i + 1) * (slot + 1) * it->focal_length;
	}

	return result;
}

int main(int argc, char** argv)
{
	const char* file_path = "/usr/share/aoc2023/day15_input.txt";

	if (argc >= 2)
		file_path = argv[1];

	FILE* fp = fopen(file_path, "r");
	if (fp == nullptr)
	{
		perror("fopen");
		return 1;
	}

	printf("puzzle1: %" PRId64 "\n", puzzle1(fp));

	fseek(fp, 0, SEEK_SET);

	printf("puzzle2: %" PRId64 "\n", puzzle2(fp));

	fclose(fp);
}
