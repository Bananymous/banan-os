#include <BAN/Array.h>
#include <BAN/String.h>
#include <BAN/Vector.h>

#include <ctype.h>
#include <stdio.h>
#include <stdint.h>

using i32 = int32_t;
using i64 = int64_t;

using u32 = uint32_t;
using u64 = uint64_t;

enum class Direction
{
	North,
	East,
	South,
	West
};

struct Position
{
	size_t x;
	size_t y;
	Direction from;
};

using Grid = BAN::Vector<BAN::Vector<u32>>;

Grid parse_grid(FILE* fp)
{
	Grid grid;
	char buffer[256];
	while (fgets(buffer, sizeof(buffer), fp))
	{
		if (strlen(buffer) < 2)
			continue;
		MUST(grid.emplace_back(strlen(buffer) - 1));
		for (size_t i = 0; buffer[i + 1]; i++)
			grid.back()[i] = buffer[i];
	}
	return grid;
}

bool can_enter_tile_from(char tile, Direction from)
{
	switch (from)
	{
		case Direction::North:
			return tile == '|' || tile == 'L' || tile == 'J';
		case Direction::South:
			return tile == '|' || tile == '7' || tile == 'F';
		case Direction::West:
			return tile == '-' || tile == 'J' || tile == '7';
		case Direction::East:
			return tile == '-' || tile == 'L' || tile == 'F';
		default:
			return false;
	}
};

Direction tile_exit_direction(char tile, Direction enter)
{
	switch (tile)
	{
		case '|': return (enter == Direction::North)	? Direction::South	: Direction::North;
		case '-': return (enter == Direction::East)		? Direction::West	: Direction::East;
		case 'L': return (enter == Direction::North)	? Direction::East	: Direction::North;
		case 'J': return (enter == Direction::North)	? Direction::West	: Direction::North;
		case '7': return (enter == Direction::South)	? Direction::West	: Direction::South;
		case 'F': return (enter == Direction::South)	? Direction::East	: Direction::South;
	}
	ASSERT_NOT_REACHED();
};

BAN::Array<Position, 2> find_grid_first_moves(const Grid& grid)
{
	BAN::Array<Position, 2> positions;
	for (size_t y = 0; y < grid.size(); y++)
	{
		for (size_t x = 0; x < grid.size(); x++)
		{
			if (grid[y][x] == 'S')
			{
				size_t index = 0;
				if (can_enter_tile_from(grid[y - 1][x], Direction::South))
					positions[index++] = { x, y - 1, Direction::South };
				if (can_enter_tile_from(grid[y + 1][x], Direction::North))
					positions[index++] = { x, y + 1, Direction::North };
				if (can_enter_tile_from(grid[y][x - 1], Direction::East))
					positions[index++] = { x - 1, y, Direction::East };
				if (can_enter_tile_from(grid[y][x + 1], Direction::West))
					positions[index++] = { x + 1, y, Direction::West };
				ASSERT(index == 2);
				return positions;
			}
		}
	}
	ASSERT_NOT_REACHED();
}

i64 puzzle1(FILE* fp)
{
	auto grid = parse_grid(fp);
	auto positions = find_grid_first_moves(grid);

	for (i64 distance = 1;; distance++)
	{
		if (positions[0].x == positions[1].x && positions[0].y == positions[1].y)
			return distance;
		
		for (auto& position : positions)
		{
			Direction direction = tile_exit_direction(grid[position.y][position.x], position.from);
			switch (direction)
			{
				case Direction::North:	position.y--; position.from = Direction::South;	break;
				case Direction::South:	position.y++; position.from = Direction::North;	break;
				case Direction::West:	position.x--; position.from = Direction::East;	break;
				case Direction::East:	position.x++; position.from = Direction::West;	break;
			}
		}
	}
}

i64 puzzle2(FILE* fp)
{
	enum Flag : u32
	{
		Path	= 1 << 8,
		Left	= 1 << 9,
		Right	= 1 << 10,
		Mask	= Path | Left | Right,
	};

	auto grid = parse_grid(fp);
	auto position = find_grid_first_moves(grid)[0];

	while ((grid[position.y][position.x] & ~Flag::Mask) != 'S')
	{	
		Direction direction = tile_exit_direction(grid[position.y][position.x] & ~Flag::Mask, position.from);

		switch (grid[position.y][position.x] & ~Flag::Mask)
		{
			case '|':
				if (position.x > 0)
					grid[position.y][position.x - 1] |= (direction == Direction::North) ? Flag::Left : Flag::Right;
				if (position.x < grid[position.y].size() - 1)
					grid[position.y][position.x + 1] |= (direction == Direction::North) ? Flag::Right : Flag::Left;
				break;
			case '-':
				if (position.y > 0)
					grid[position.y - 1][position.x] |= (direction == Direction::East) ? Flag::Left : Flag::Right;
				if (position.y < grid.size() - 1)
					grid[position.y + 1][position.x] |= (direction == Direction::East) ? Flag::Right : Flag::Left;
				break;
			case 'L':
				if (position.x > 0)
					grid[position.y][position.x - 1] |= (direction == Direction::North) ? Flag::Left : Flag::Right;
				if (position.y < grid.size() - 1)
					grid[position.y + 1][position.x] |= (direction == Direction::North) ? Flag::Left : Flag::Right;
				break;
			case 'J':
				if (position.x < grid[position.y].size() - 1)
					grid[position.y][position.x + 1] |= (direction == Direction::West) ? Flag::Left : Flag::Right;
				if (position.y < grid.size() - 1)
					grid[position.y + 1][position.x] |= (direction == Direction::West) ? Flag::Left : Flag::Right;
				break;
			case '7':
				if (position.y > 0)
					grid[position.y - 1][position.x] |= (direction == Direction::South) ? Flag::Left : Flag::Right;
				if (position.x < grid[position.y].size() - 1)
					grid[position.y][position.x + 1] |= (direction == Direction::South) ? Flag::Left : Flag::Right;
				break;
			case 'F':
				if (position.y > 0)
					grid[position.y - 1][position.x] |= (direction == Direction::East) ? Flag::Left : Flag::Right;
				if (position.x > 0)
					grid[position.y][position.x - 1] |= (direction == Direction::East) ? Flag::Left : Flag::Right;
				break;
		}

		grid[position.y][position.x] |= Flag::Path;

		switch (direction)
		{
			case Direction::North:	position.y--; position.from = Direction::South;	break;
			case Direction::South:	position.y++; position.from = Direction::North;	break;
			case Direction::West:	position.x--; position.from = Direction::East;	break;
			case Direction::East:	position.x++; position.from = Direction::West;	break;
		}
	}

	// Mark start tile as part of the path
	grid[position.y][position.x] |= Flag::Path;
	
	// Clean up flags
	for (auto& row : grid)
	{
		for (u32& tile : row)
		{
			// Remove left and right from path
			if (tile & Flag::Path)
				tile &= ~(Flag::Left | Flag::Right);
			// Tile should never be both left and right
			ASSERT(!((tile & Flag::Left) && (tile & Flag::Right)));
		}
	}

	// Determine whether left or right is enclosed by loop
	Flag enclosed = Flag::Path;
	for (const auto& row : grid)
	{
		for (u32 tile : row)
		{
			if ((tile & (Flag::Right | Flag::Left)))
			{
				enclosed = (tile & Flag::Right) ? Flag::Left : Flag::Right;
				break;
			}
		}
		if (enclosed != Flag::Path)
			break;
	}
	ASSERT(enclosed != Flag::Path);

	// Expand all enclosed areas
	bool modified = true;
	while (modified)
	{
		modified = false;
		for (size_t y = 1; y < grid.size(); y++)
		{
			for (size_t x = 1; x < grid[y].size(); x++)
			{
				if (grid[y][x] & Flag::Mask)
					continue;
				if ((grid[y - 1][x] & enclosed) || (grid[y][x - 1] & enclosed))
				{
					grid[y][x] |= enclosed;
					modified = true;
				}
			}
		}
	}

	// Calculate number of enclosed tiles
	i64 result = 0;
	for (const auto& row : grid)
		for (u32 c : row)
			result += !!(c & enclosed);
	return result;
}

int main(int argc, char** argv)
{
	const char* file_path = "/usr/share/aoc2023/day10_input.txt";

	if (argc >= 2)
		file_path = argv[1];

	FILE* fp = fopen(file_path, "r");
	if (fp == nullptr)
	{
		perror("fopen");
		return 1;
	}

	printf("puzzle1: %lld\n", puzzle1(fp));

	fseek(fp, 0, SEEK_SET);

	printf("puzzle2: %lld\n", puzzle2(fp));

	fclose(fp);
}
