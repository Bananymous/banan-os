#include <BAN/HashSet.h>
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

enum class Tile { Empty, PositiveMirror, NegativeMirror, HorizontalSplitter, VerticalSplitter };
enum class Direction { North, South, East, West };

using Grid = BAN::Vector<BAN::Vector<Tile>>;

struct Position
{
	i64 y;
	i64 x;
	Direction dir;

	bool operator==(const Position& other) const
	{
		return x == other.x && y == other.y && dir == other.dir;
	}
};

struct PositionHash
{
	BAN::hash_t operator()(Position position) const
	{
		return BAN::hash<i64>()((position.y << 32) | position.x) ^ BAN::hash<i8>()(static_cast<i8>(position.dir));
	}
};

Grid parse_grid(FILE* fp)
{
	Grid grid;

	auto char_to_tile =
		[](char c)
		{
			if (c == '.')
				return Tile::Empty;
			if (c == '/')
				return Tile::PositiveMirror;
			if (c == '\\')
				return Tile::NegativeMirror;
			if (c == '-')
				return Tile::HorizontalSplitter;
			if (c == '|')
				return Tile::VerticalSplitter;
			ASSERT_NOT_REACHED();
		};

	char buffer[128];
	while (fgets(buffer, sizeof(buffer), fp))
	{
		BAN::StringView line(buffer);
		ASSERT(line.back() == '\n');
		line = line.substring(0, line.size() - 1);
		if (line.empty())
			break;

		MUST(grid.emplace_back(line.size()));
		for (size_t i = 0; i < line.size(); i++)
			grid.back()[i] = char_to_tile(line[i]);
	}

	return grid;
}

BAN::Vector<Position> get_next_positions(Position position, const Grid& grid)
{
	auto tile = grid[position.y][position.x];

	BAN::Vector<Position> next_positions;

	switch (tile)
	{
		case Tile::Empty:
			switch (position.dir)
			{
				case Direction::North:	MUST(next_positions.emplace_back(position.y - 1, position.x,     Direction::North)); break;
				case Direction::South:	MUST(next_positions.emplace_back(position.y + 1, position.x,     Direction::South)); break;
				case Direction::West:	MUST(next_positions.emplace_back(position.y,     position.x - 1, Direction::West)); break;
				case Direction::East:	MUST(next_positions.emplace_back(position.y,     position.x + 1, Direction::East)); break;
			}
			break;
		case Tile::PositiveMirror:
			switch (position.dir)
			{
				case Direction::North:	MUST(next_positions.emplace_back(position.y,     position.x + 1, Direction::East)); break;
				case Direction::South:	MUST(next_positions.emplace_back(position.y,     position.x - 1, Direction::West)); break;
				case Direction::West:	MUST(next_positions.emplace_back(position.y + 1, position.x,     Direction::South)); break;
				case Direction::East:	MUST(next_positions.emplace_back(position.y - 1, position.x,     Direction::North)); break;
			}
			break;
		case Tile::NegativeMirror:
			switch (position.dir)
			{
				case Direction::North:	MUST(next_positions.emplace_back(position.y,     position.x - 1, Direction::West)); break;
				case Direction::South:	MUST(next_positions.emplace_back(position.y,     position.x + 1, Direction::East)); break;
				case Direction::West:	MUST(next_positions.emplace_back(position.y - 1, position.x,     Direction::North)); break;
				case Direction::East:	MUST(next_positions.emplace_back(position.y + 1, position.x,     Direction::South)); break;
			}
			break;
		case Tile::HorizontalSplitter:
			switch (position.dir)
			{
				case Direction::North:
				case Direction::South:
					MUST(next_positions.emplace_back(position.y, position.x - 1, Direction::West));
					MUST(next_positions.emplace_back(position.y, position.x + 1, Direction::East));
					break;
				case Direction::West:	MUST(next_positions.emplace_back(position.y, position.x - 1, Direction::West)); break;
				case Direction::East:	MUST(next_positions.emplace_back(position.y, position.x + 1, Direction::East)); break;
			}
			break;
		case Tile::VerticalSplitter:
			switch (position.dir)
			{
				case Direction::North:	MUST(next_positions.emplace_back(position.y - 1, position.x, Direction::North)); break;
				case Direction::South:	MUST(next_positions.emplace_back(position.y + 1, position.x, Direction::South)); break;
				case Direction::West:
				case Direction::East:
					MUST(next_positions.emplace_back(position.y - 1, position.x, Direction::North));
					MUST(next_positions.emplace_back(position.y + 1, position.x, Direction::South));
					break;
			}
			break;
	}

	for (size_t i = 0; i < next_positions.size();)
	{
		if (next_positions[i].y < 0 || next_positions[i].y >= (i64)grid.size())
			next_positions.remove(i);
		else if (next_positions[i].x < 0 || next_positions[i].x >= (i64)grid.front().size())
			next_positions.remove(i);
		else
			i++;
	}

	return next_positions;
}

i64 count_energized_tiles(const Grid& grid, Position start)
{
	BAN::HashSet<Position, PositionHash> visited;
	BAN::HashSet<Position, PositionHash> energized;

	BAN::Vector<Position> current_positions;
	MUST(current_positions.push_back(start));
	MUST(visited.insert(current_positions.front()));

	while (!current_positions.empty())
	{
		auto position = current_positions.back();
		current_positions.pop_back();

		MUST(energized.insert({ position.y, position.x, Direction::North }));

		auto next_positions = get_next_positions(position, grid);
		for (auto next : next_positions)
		{
			if (visited.contains(next))
				continue;
			MUST(visited.insert(next));
			MUST(current_positions.push_back(next));
		}
	}

	return energized.size();
}

i64 puzzle1(FILE* fp)
{
	auto grid = parse_grid(fp);
	return count_energized_tiles(grid, { 0, 0, Direction::East });
}

i64 puzzle2(FILE* fp)
{
	auto grid = parse_grid(fp);

	i64 max_energized = 0;

	for (i64 y = 0; y < (i64)grid.size(); y++)
	{
		max_energized = BAN::Math::max(max_energized, count_energized_tiles(grid, { y, 0, Direction::East }));
		max_energized = BAN::Math::max(max_energized, count_energized_tiles(grid, { y, (i64)grid.front().size() - 1, Direction::West }));
	}

	for (i64 x = 0; x < (i64)grid.front().size(); x++)
	{
		max_energized = BAN::Math::max(max_energized, count_energized_tiles(grid, { 0, x, Direction::South }));
		max_energized = BAN::Math::max(max_energized, count_energized_tiles(grid, { (i64)grid.size() - 1, x, Direction::North }));
	}

	return max_energized;
}

int main(int argc, char** argv)
{
	const char* file_path = "/usr/share/aoc2023/day16_input.txt";

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
