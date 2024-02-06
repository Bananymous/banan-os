#include <BAN/HashSet.h>
#include <BAN/StringView.h>

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
	i32 x;
	i32 y;

	bool operator==(const Position& other) const
	{
		return x == other.x && y == other.y;
	}

	Position& operator+=(const Position& other)
	{
		x += other.x;
		y += other.y;
		return *this;
	}

	Position operator+(const Position& other) const
	{
		Position temp = *this;
		temp += other;
		return temp;
	}
};

struct PositionHash
{
	BAN::hash_t operator()(const Position& position) const
	{
		return BAN::u64_hash(((u64)position.x << 32) | (u64)position.y);
	}
};

enum Direction { North, West, South, East };

static constexpr Direction char_to_dir(char c)
{
	if (c == 'U')
		return Direction::North;
	if (c == 'D')
		return Direction::South;
	if (c == 'L')
		return Direction::West;
	if (c == 'R')
		return Direction::East;
	ASSERT_NOT_REACHED();
}

static constexpr Position s_dir_offset[] {
	{ .x =  0, .y = -1 },
	{ .x = -1, .y =  0 },
	{ .x =  0, .y =  1 },
	{ .x =  1, .y =  0 },
};

i64 solve_general(FILE* fp, auto parse_dir, auto parse_count)
{
	BAN::HashSet<Position, PositionHash> path;
	BAN::HashSet<Position, PositionHash> lpath;
	BAN::HashSet<Position, PositionHash> rpath;

	Position current_pos { 0, 0 };
	MUST(path.insert(current_pos));

	char buffer[128];
	while (fgets(buffer, sizeof(buffer), fp))
	{
		BAN::StringView line(buffer);
		ASSERT(line.back() == '\n');
		line = line.substring(0, line.size() - 1);
		if (line.empty())
			break;

		auto dir = parse_dir(line);
		i64 count = parse_count(line);

		Position loff, roff;
		switch (dir)
		{
			case Direction::North:
				loff = s_dir_offset[Direction::West];
				roff = s_dir_offset[Direction::East];
				break;
			case Direction::South:
				loff = s_dir_offset[Direction::East];
				roff = s_dir_offset[Direction::West];
				break;
			case Direction::East:
				loff = s_dir_offset[Direction::North];
				roff = s_dir_offset[Direction::South];
				break;
			case Direction::West:
				loff = s_dir_offset[Direction::South];
				roff = s_dir_offset[Direction::North];
				break;
		}

		for (i64 i = 0; i < count; i++)
		{
			current_pos += s_dir_offset[dir];
			MUST(path.insert(current_pos));
			MUST(lpath.insert(current_pos + loff));
			MUST(rpath.insert(current_pos + roff));
		}
	}

	for (auto position : path)
	{
		if (lpath.contains(position))
			lpath.remove(position);
		if (rpath.contains(position))
			rpath.remove(position);
	}

	auto find_min_and_remove_duplicates =
		[](auto& source, auto& destination, i32& minimum)
		{
			for (auto it = source.begin(); it != source.end();)
			{
				if (!destination.contains(*it))
				{
					minimum = BAN::Math::min(minimum, it->x);
					it++;
				}
				else
				{
					source.remove(*it);
					destination.remove(*it);
					it = source.begin();
				}
			}
		};

	i32 lmin_x = INT32_MAX;
	find_min_and_remove_duplicates(lpath, rpath, lmin_x);

	i32 rmin_x = INT32_MAX;
	find_min_and_remove_duplicates(rpath, lpath, rmin_x);

	ASSERT(lmin_x != rmin_x);
	auto& expand = (lmin_x < rmin_x) ? rpath : lpath;

	BAN::HashSet<Position, PositionHash> visited;
	BAN::HashSet<Position, PositionHash> inner_area;

	while (!expand.empty())
	{
		auto position = *expand.begin();
		expand.remove(position);

		MUST(inner_area.insert(position));
		MUST(visited.insert(position));

		for (i8 dir = 0; dir < 4; dir++)
		{
			auto next = position + s_dir_offset[dir];
			if (visited.contains(next) || path.contains(next))
				continue;
			MUST(expand.insert(next));
			MUST(visited.insert(next));
		}
	}

	return path.size() + inner_area.size();
}

i64 puzzle1(FILE* fp)
{
	auto parse_dir =
		[](auto line)
		{
			if (line[0] == 'U')
				return Direction::North;
			if (line[0] == 'D')
				return Direction::South;
			if (line[0] == 'L')
				return Direction::West;
			if (line[0] == 'R')
				return Direction::East;
			ASSERT_NOT_REACHED();
		};

	auto parse_count =
		[](auto line)
		{
			i64 result = 0;
			for (size_t i = 2; i < line.size() && isdigit(line[i]); i++)
				result = (result * 10) + (line[i] - '0');
			return result;
		};

	return solve_general(fp, parse_dir, parse_count);
}

i64 puzzle2(FILE* fp)
{
	//(#7a21e3)

	auto parse_dir =
		[](auto line)
		{
			line = line.substring(*line.find('('));
			if (line[7] == '0')
				return Direction::East;
			if (line[7] == '1')
				return Direction::South;
			if (line[7] == '2')
				return Direction::West;
			if (line[7] == '3')
				return Direction::North;
			ASSERT_NOT_REACHED();
		};

	auto parse_count =
		[](auto line)
		{
			line = line.substring(*line.find('('));
			i64 result = 0;
			for (size_t i = 2; i < 7; i++)
				result = (result * 16) + ((isdigit(line[i])) ? (line[i] - '0') : (tolower(line[i] - 'a' + 10)));
			return result;
		};

	return solve_general(fp, parse_dir, parse_count);
}

int main(int argc, char** argv)
{
	const char* file_path = "/usr/share/aoc2023/day18_input.txt";

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
