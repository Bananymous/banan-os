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

struct Hailstone
{
	double px, py, pz;
	double vx, vy, vz;
};

i64 parse_i64(BAN::StringView str)
{
	while (isspace(str.front()))
		str = str.substring(1);
	bool negative = str.front() == '-';
	if (negative)
		str = str.substring(1);
	i64 result = 0;
	for (char c : str)
		if (isdigit(c))
			result = (result * 10) + (c - '0');
	return negative ? -result : result;
}

BAN::Vector<Hailstone> parse_hailstones(FILE* fp)
{
	BAN::Vector<Hailstone> hailstones;

	char buffer[128];
	while (fgets(buffer, sizeof(buffer), fp))
	{
		BAN::StringView line(buffer);
		ASSERT(line.back() == '\n');
		line = line.substring(0, line.size() - 1);
		if (line.empty())
			break;

		auto position_velocity_strs = MUST(line.split('@'));
		ASSERT(position_velocity_strs.size() == 2);

		Hailstone hailstone;

		auto position_strs = MUST(position_velocity_strs[0].split(','));
		ASSERT(position_strs.size() == 3);
		hailstone.px = parse_i64(position_strs[0]);
		hailstone.py = parse_i64(position_strs[1]);
		hailstone.pz = parse_i64(position_strs[2]);

		auto velocity_strs = MUST(position_velocity_strs[1].split(','));
		ASSERT(velocity_strs.size() == 3);
		hailstone.vx = parse_i64(velocity_strs[0]);
		hailstone.vy = parse_i64(velocity_strs[1]);
		hailstone.vz = parse_i64(velocity_strs[2]);

		MUST(hailstones.push_back(hailstone));
	}

	return hailstones;
}

i64 puzzle1(FILE* fp)
{
	auto hailstones = parse_hailstones(fp);

	i64 result = 0;

	for (size_t i = 0; i < hailstones.size(); i++)
	{
		for (size_t j = i + 1; j < hailstones.size(); j++)
		{
			const auto& h1 = hailstones[i];
			const auto& h2 = hailstones[j];

			// skip parallel lines
			if (h1.vy * h2.vx == h2.vy * h1.vx)
				continue;

			const double t2 = ((h2.py - h1.py) * h1.vx - (h2.px - h1.px) * h1.vy) / (h2.vx * h1.vy - h1.vx * h2.vy);
			if (t2 < 0.0)
				continue;

			const double t1 = ((h2.py - h1.py) + t2 * h2.vy) / h1.vy;
			if (t1 < 0.0)
				continue;

			const double x = h2.px + t2 * h2.vx;
			const double y = h2.py + t2 * h2.vy;

			constexpr double min = 200000000000000.0;
			constexpr double max = 400000000000000.0;
			if ((min <= x && x <= max) && (min <= y && y <= max))
				result++;
		}
	}

	return result;
}

i64 puzzle2(FILE* fp)
{
	(void)fp;
	return -1;
}

int main(int argc, char** argv)
{
	const char* file_path = "/usr/share/aoc2023/day24_input.txt";

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
