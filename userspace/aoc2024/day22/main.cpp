#include <BAN/HashMap.h>
#include <BAN/HashSet.h>
#include <BAN/Vector.h>

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>

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

static constexpr u64 generate_next_secret(u64 secret)
{
	secret = (secret ^ (secret <<  6)) & 0xFFFFFF;
	secret = (secret ^ (secret >>  5)) & 0xFFFFFF;
	secret = (secret ^ (secret << 11)) & 0xFFFFFF;
	return secret;
}

i64 part1(FILE* fp)
{
	u64 result = 0;

	char buffer[128];
	while (fgets(buffer, sizeof(buffer), fp))
	{
		u64 secret = atoll(buffer);
		for (usize i = 0; i < 2000; i++)
			secret = generate_next_secret(secret);
		result += secret;
	}

	return result;
}

i64 part2(FILE* fp)
{
	union Sequence
	{
		u32 raw;
		i8 diffs[4];
	};

	BAN::HashMap<u32, u64> sequence_to_bananas;

	char buffer[128];
	while (fgets(buffer, sizeof(buffer), fp))
	{
		BAN::Vector<u8> values;
		MUST(values.reserve(2001));

		u64 secret = atoll(buffer);
		MUST(values.push_back(secret % 10));
		for (usize i = 0; i < 2000; i++)
		{
			secret = generate_next_secret(secret);
			MUST(values.push_back(secret % 10));
		}

		BAN::HashSet<u32> found_sequences;

		u32 sequence =
			((u32)(u8)(values[1] - values[0]) <<  8) |
			((u32)(u8)(values[2] - values[1]) << 16) |
			((u32)(u8)(values[3] - values[2]) << 24);
		for (usize i = 4; i < values.size(); i++)
		{
			sequence = (sequence >> 8) | ((u32)(u8)(values[i] - values[i - 1]) << 24);

			if (found_sequences.contains(sequence))
				continue;
			MUST(found_sequences.insert(sequence));

			auto it = sequence_to_bananas.find(sequence);
			if (it == sequence_to_bananas.end())
				it = MUST(sequence_to_bananas.insert(sequence, 0));
			it->value += values[i];
		}
	}

	u64 result = 0;
	for (auto [_, bananas] : sequence_to_bananas)
		result = BAN::Math::max(result, bananas);
	return result;
}

int main(int argc, char** argv)
{
	const char* file_path = "/usr/share/aoc2024/day22_input.txt";

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
