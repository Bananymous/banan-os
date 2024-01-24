#include <BAN/HashSet.h>
#include <BAN/Vector.h>
#include <BAN/Hash.h>

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
	i64 x;
	i64 y;

	bool operator==(const Position& other) const
	{
		return x == other.x && y == other.y;
	}
};


namespace BAN
{
	template<>
	struct hash<Position>
	{
		hash_t operator()(const Position& position) const
		{
			return hash<u64>()(((u64)position.x << 32) | (u64)position.y);
		}
	};
}

BAN::Vector<BAN::Vector<i64>> parse_grid(FILE* fp)
{
	BAN::Vector<BAN::Vector<i64>> grid;

	char buffer[256];
	while (fgets(buffer, sizeof(buffer), fp))
	{
		BAN::StringView line(buffer);
		ASSERT(line.back() == '\n');
		line = line.substring(0, line.size() - 1);
		if (line.empty())
			break;

		MUST(grid.emplace_back(line.size()));
		for (size_t i = 0; i < line.size(); i++)
			grid.back()[i] = line[i] - '0';
	}

	return grid;
}

//static constexpr i64 MIN_STEPS = 4;
//static constexpr i64 MAX_STEPS = 10;
template<i64 MIN_STEPS, i64 MAX_STEPS>
i64 solve_general(FILE* fp)
{
	struct Block
	{
		// heatloss[x][y]:
		// x: direction
		// y: steps left in that direction
		i64 heatloss[4][MAX_STEPS + 1];
		i8 entered_from = -1;
	};

	const auto grid = parse_grid(fp);

	// initially mark everything very large, except (0, 0)
	BAN::Vector<BAN::Vector<Block>> heatloss_map;
	MUST(heatloss_map.resize(grid.size()));
	for (size_t y = 0; y < grid.size(); y++)
	{
		MUST(heatloss_map[y].resize(grid[y].size()));
		for (size_t x = 0; x < grid[y].size(); x++)
			for (size_t dir = 0; dir < 4; dir++)
				for (size_t step = 0; step <= MAX_STEPS; step++)
					heatloss_map[y][x].heatloss[dir][step] = 1'000'000'000;
	}
	for (size_t dir = 0; dir < 4; dir++)
		for (size_t step = 0; step <= MAX_STEPS; step++)
			heatloss_map[0][0].heatloss[dir][step] = 0;

	BAN::HashSet<Position> visited;
	BAN::HashSet<Position> pending;
	MUST(pending.insert({ 0, 0 }));

	while (!pending.empty())
	{
		auto position = *pending.begin();
		pending.remove(position);

		Position offsets[4] = { { -1, 0 }, { 0, -1 }, { 1, 0 }, { 0, 1 } };

		for (i8 dir = 0; dir < 4; dir++)
		{
			auto target = position;

			i64 path_heatloss = 0;

			auto is_target_in_bounds =
				[&](const Position& target)
				{
					if (target.y < 0 || target.y >= (i64)grid.size())
						return false;
					if (target.x < 0 || target.x >= (i64)grid.front().size())
						return false;
					return true;
				};

			i64 target_distance = (heatloss_map[position.y][position.x].entered_from == dir) ? 1 : MIN_STEPS;
			for (i64 i = 0; i < target_distance; i++)
			{
				target.x += offsets[dir].x;
				target.y += offsets[dir].y;
				if (!is_target_in_bounds(target))
					break;
				path_heatloss += grid[target.y][target.x];
			}
			if (!is_target_in_bounds(target))
				continue;

			auto& target_heatloss = heatloss_map[target.y][target.x];

			bool target_updated = false;
			for (i8 new_dir = 0; new_dir < 4; new_dir++)
			{
				// Don't allow going backwards
				if (new_dir == dir + 2 || dir == new_dir + 2)
					continue;

				for (i64 step = target_distance; step <= MAX_STEPS; step++)
				{
					i64 possible_heatloss = heatloss_map[position.y][position.x].heatloss[dir][step] + path_heatloss;
					i64 new_dir_max_step = (new_dir == dir) ? step - target_distance : MAX_STEPS;
					for (i64 i = 0; i <= new_dir_max_step; i++)
					{
						if (possible_heatloss >= target_heatloss.heatloss[new_dir][i])
							continue;
						target_heatloss.heatloss[new_dir][i] = possible_heatloss;
						target_heatloss.entered_from = dir;
						target_updated = true;
					}
				}
			}
			if (target_updated || !visited.contains(target))
				MUST(pending.insert(target));
		}

		MUST(visited.insert(position));
	}

	i64 result = INT64_MAX;
	for (size_t dir = 0; dir < 4; dir++)
		for (size_t step = 0; step <= MAX_STEPS; step++)
			result = BAN::Math::min(result, heatloss_map.back().back().heatloss[dir][step]);
	return result;
}

i64 puzzle1(FILE* fp)
{
	return solve_general<1, 3>(fp);
}

i64 puzzle2(FILE* fp)
{
	return solve_general<4, 10>(fp);
}

int main(int argc, char** argv)
{
	const char* file_path = "/usr/share/aoc2023/day17_input.txt";

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
