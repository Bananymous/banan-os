#include <BAN/HashMap.h>
#include <BAN/Vector.h>
#include <BAN/Sort.h>

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
	BAN::Vector<i64> lhs;
	BAN::Vector<i64> rhs;

	for (;;) {
		i64 l, r;
		if (fscanf(fp, "%" SCNd64 " %" SCNd64 "\n", &l, &r) != 2)
			break;
		MUST(lhs.push_back(l));
		MUST(rhs.push_back(r));
	}

	BAN::sort::sort(lhs.begin(), lhs.end());
	BAN::sort::sort(rhs.begin(), rhs.end());

	i64 result = 0;
	for (size_t i = 0; i < lhs.size(); i++)
		result += BAN::Math::abs(lhs[i] - rhs[i]);

	return result;
}

i64 part2(FILE* fp)
{
	BAN::HashMap<i64, i64> lhs;
	BAN::HashMap<i64, i64> rhs;

	for (;;) {
		i64 l, r;
		if (fscanf(fp, "%" SCNd64 " %" SCNd64 "\n", &l, &r) != 2)
			break;

		{
			auto it = lhs.find(l);
			if (it == lhs.end())
				MUST(lhs.insert(l, 1));
			else
				it->value++;
		}

		{
			auto it = rhs.find(r);
			if (it == rhs.end())
				MUST(rhs.insert(r, 1));
			else
				it->value++;
		}
	}

	i64 result = 0;

	for (auto [id, count] : lhs) {
		auto it = rhs.find(id);
		if (it == rhs.end())
			continue;
		result += id * count * it->value;
	}

	return result;
}

int main(int argc, char** argv)
{
	const char* file_path = "/usr/share/aoc2024/day1_input.txt";

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
