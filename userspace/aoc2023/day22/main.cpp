#include <BAN/HashSet.h>
#include <BAN/Sort.h>
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

struct Position
{
	i64 x, y, z;

	bool operator==(const Position& other) const
	{
		return x == other.x && y == other.y && z == other.z;
	}
};

static constexpr bool rectangle_contains(const Position& c1, const Position& c2, const Position& p)
{
	ASSERT(c1.x <= c2.x);
	ASSERT(c1.y <= c2.y);
	return (c1.x <= p.x && p.x <= c2.x) && (c1.y <= p.y && p.y <= c2.y);
}

struct Brick
{
	Position corners[2];
	BAN::HashSet<Brick*> supporting;
	BAN::HashSet<Brick*> supported_by;

	bool supports(const Brick& other) const
	{
		if (corners[1].z + 1 != other.corners[0].z)
			return false;

		for (i32 i = 0; i < 4; i++)
			if (rectangle_contains(corners[0], corners[1], { other.corners[i / 2].x, other.corners[i % 2].y, 0 }))
				return true;

		for (i32 i = 0; i < 4; i++)
			if (rectangle_contains(other.corners[0], other.corners[1], { corners[i / 2].x, corners[i % 2].y, 0 }))
				return true;

		return false;
	}
};

i64 parse_i64(BAN::StringView str)
{
	i64 result = 0;
	for (char c : str)
	{
		ASSERT(isdigit(c));
		result = (result * 10) + (c - '0');
	}
	return result;
}

BAN::Vector<Brick> parse_bricks(FILE* fp)
{
	BAN::Vector<Brick> bricks;

	char buffer[64];
	while (fgets(buffer, sizeof(buffer), fp))
	{
		BAN::StringView line(buffer);
		ASSERT(line.back() == '\n');
		line = line.substring(0, line.size() - 1);
		if (line.empty())
			break;

		auto corner_strs = MUST(line.split('~'));
		ASSERT(corner_strs.size() == 2);

		Brick brick;
		for (i32 i = 0; i < 2; i++)
		{
			auto coords = MUST(corner_strs[i].split(','));
			ASSERT(coords.size() == 3);
			brick.corners[i].x = parse_i64(coords[0]);
			brick.corners[i].y = parse_i64(coords[1]);
			brick.corners[i].z = parse_i64(coords[2]);
		}
		ASSERT(brick.corners[0].x <= brick.corners[1].x);
		ASSERT(brick.corners[0].y <= brick.corners[1].y);
		ASSERT(brick.corners[0].z <= brick.corners[1].z);
		MUST(bricks.push_back(brick));
	}

	return bricks;
}

i64 puzzle1(FILE* fp)
{
	auto brick_comp = [](const Brick& b1, const Brick& b2) { return b1.corners[0].z < b2.corners[0].z; };

	auto bricks = parse_bricks(fp);
	BAN::sort::sort(bricks.begin(), bricks.end(), brick_comp);

	// Simulate brick falling
	for (size_t i = 0; i < bricks.size();)
	{
		bool can_fall = bricks[i].corners[0].z > 1;
		for (size_t j = 0; j < i && can_fall; j++)
			if (bricks[j].supports(bricks[i]))
				can_fall = false;

		if (!can_fall)
			i++;
		else
		{
			bricks[i].corners[0].z--;
			bricks[i].corners[1].z--;
			for (; i > 0; i--)
			{
				if (brick_comp(bricks[i - 1], bricks[i]))
					break;
				BAN::swap(bricks[i - 1], bricks[i]);
			}
		}
	}

	// Store brick supporting structures
	for (size_t i = 0; i < bricks.size(); i++)
	{
		for (size_t j = 0; j < bricks.size(); j++)
		{
			if (i == j)
				continue;
			if (bricks[i].supports(bricks[j]))
			{
				MUST(bricks[i].supporting.insert(&bricks[j]));
				MUST(bricks[j].supported_by.insert(&bricks[i]));
			}
		}
	}

	i64 result = 0;
	for (const auto& brick : bricks)
	{
		bool disintegratable = true;
		for (const auto* support : brick.supporting)
			if (support->supported_by.size() <= 1)
				disintegratable = false;
		result += disintegratable;
	}

	// OFF BY 7

	return result;
}

i64 puzzle2(FILE* fp)
{
	(void)fp;
	return -1;
}

int main(int argc, char** argv)
{
	const char* file_path = "/usr/share/aoc2023/day22_input.txt";

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
