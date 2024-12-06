#include <BAN/HashMap.h>
#include <BAN/HashSet.h>
#include <BAN/Optional.h>
#include <BAN/Vector.h>

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
	u32 x, y;

	constexpr bool operator==(const Position& other) const
	{
		return x == other.x && y == other.y;
	}
};

struct PositionHash
{
	constexpr BAN::hash_t operator()(Position state) const
	{
		return BAN::hash<u64>{}((u64)state.x << 32 | state.y);
	}
};

struct GuardState
{
	enum Dir : uint8_t
	{
		UP      = 0,
		RIGHT   = 1,
		DOWN    = 2,
		LEFT    = 3,

		INVALID = 0xFF,
	};

	Position pos;
	Dir dir;

	constexpr bool operator==(const GuardState& other) const
	{
		return pos == other.pos && dir == other.dir;
	}
};

struct GuardStateHash
{
	constexpr BAN::hash_t operator()(GuardState state) const
	{
		return PositionHash()(state.pos) ^ BAN::hash<u32>()(state.dir);
	}
};

struct ParseInputResult
{
	BAN::Vector<BAN::Vector<char>> map;
	GuardState guard_state;
};

static ParseInputResult parse_input(FILE* fp)
{
	BAN::Vector<BAN::Vector<char>> map;

	char buffer[1024];
	while (fgets(buffer, sizeof(buffer), fp))
	{
		if (!buffer[0] || buffer[0] == '\n')
			break;

		const size_t row_len = strlen(buffer) - 1;
		ASSERT(buffer[row_len] == '\n');

		if (!map.empty())
			ASSERT(map.back().size() == row_len);

		BAN::Vector<char> row;
		MUST(row.resize(row_len));
		memcpy(row.data(), buffer, row_len);

		MUST(map.push_back(BAN::move(row)));
	}

	for (size_t y = 0; y < map.size(); y++)
	{
		for (size_t x = 0; x < map[y].size(); x++)
		{
			GuardState state;
			state.dir = GuardState::Dir::INVALID;
			state.pos.x = x;
			state.pos.y = y;

			switch (map[y][x])
			{
				case '^': state.dir = GuardState::Dir::UP;    break;
				case '>': state.dir = GuardState::Dir::RIGHT; break;
				case '<': state.dir = GuardState::Dir::LEFT;  break;
				case 'v': state.dir = GuardState::Dir::DOWN;  break;
			}

			if (state.dir != GuardState::Dir::INVALID)
				return { .map = BAN::move(map), .guard_state = state };
		}
	}

	ASSERT_NOT_REACHED();
}

static BAN::Optional<GuardState> get_next_guard_state(const BAN::Vector<BAN::Vector<char>>& map, const GuardState& guard_state)
{
	Position next_pos = guard_state.pos;

	switch (guard_state.dir)
	{
		case GuardState::Dir::UP:    next_pos.y--; break;
		case GuardState::Dir::RIGHT: next_pos.x++; break;
		case GuardState::Dir::LEFT:  next_pos.x--; break;
		case GuardState::Dir::DOWN:  next_pos.y++; break;
		default: ASSERT_NOT_REACHED();
	}

	if (next_pos.y >= map.size() || next_pos.x >= map[next_pos.y].size())
		return {};

	GuardState next_state;

	if (map[next_pos.y][next_pos.x] == '#')
	{
		next_state.pos = guard_state.pos;
		next_state.dir = static_cast<GuardState::Dir>((guard_state.dir + 1) % 4);
	}
	else
	{
		next_state.pos = next_pos;
		next_state.dir = guard_state.dir;
	}

	return next_state;
}

i64 part1(FILE* fp)
{
	auto [map, guard] = parse_input(fp);

	BAN::HashSet<Position, PositionHash> visited;

	for (;;)
	{
		MUST(visited.insert(guard.pos));

		auto opt_next_guard = get_next_guard_state(map, guard);
		if (!opt_next_guard.has_value())
			break;
		guard = opt_next_guard.release_value();
	}

	return visited.size();
}

static bool does_guard_enter_a_loop(const BAN::Vector<BAN::Vector<char>>& map, const GuardState& guard_state)
{
	BAN::HashSet<GuardState, GuardStateHash> visited;

	GuardState guard = guard_state;
	for (;;)
	{
		if (visited.contains(guard))
			return true;
		MUST(visited.insert(guard));

		auto opt_next_guard = get_next_guard_state(map, guard);
		if (!opt_next_guard.has_value())
			return false;
		guard = opt_next_guard.release_value();
	}
}

i64 part2(FILE* fp)
{
	auto [map, initial_guard] = parse_input(fp);

	BAN::HashMap<Position, GuardState, PositionHash> to_check;

	auto guard = initial_guard;
	for (;;)
	{
		auto opt_next_guard = get_next_guard_state(map, guard);
		if (!opt_next_guard.has_value())
			break;

		auto next_guard = opt_next_guard.release_value();

		if (next_guard.pos != initial_guard.pos && !to_check.contains(next_guard.pos))
			MUST(to_check.insert(next_guard.pos, guard));

		guard = next_guard;
	}

	i64 result = 0;

	for (const auto& [position, guard_state] : to_check)
	{
		map[position.y][position.x] = '#';
		if (does_guard_enter_a_loop(map, guard_state))
			result++;
		map[position.y][position.x] = '.';
	}

	return result;
}

int main(int argc, char** argv)
{
	const char* file_path = "/usr/share/aoc2024/day6_input.txt";

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
