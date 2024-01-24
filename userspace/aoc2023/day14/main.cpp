#include <BAN/Array.h>
#include <BAN/HashMap.h>
#include <BAN/String.h>
#include <BAN/Swap.h>
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

enum class Tile { Empty, RollableRock, StaticRock };
enum class Direction { North, South, West, East };

using Platform = BAN::Vector<BAN::Vector<Tile>>;

Platform parse_platform(FILE* fp)
{
	Platform platform;

	char buffer[128];
	while (fgets(buffer, sizeof(buffer), fp))
	{
		BAN::StringView line(buffer);
		ASSERT(line.back() == '\n');
		line = line.substring(0, line.size() - 1);
		if (line.empty())
			continue;

		auto char_to_tile =
			[](char c)
			{
				if (c == '.')
					return Tile::Empty;
				if (c == 'O')
					return Tile::RollableRock;
				if (c == '#')
					return Tile::StaticRock;
				ASSERT_NOT_REACHED();
			};

		MUST(platform.emplace_back(line.size()));
		for (size_t i = 0; i < line.size(); i++)
			platform.back()[i] = char_to_tile(line[i]);
	}

	return platform;
}

void tilt_platform(Platform& platform, Direction direction)
{
	switch (direction)
	{
		case Direction::North:
			for (size_t y = 0; y < platform.size(); y++)
				for (size_t x = 0; x < platform.front().size(); x++)
					if (platform[y][x] == Tile::RollableRock)
						for (size_t y_off = 1; y_off <= y && platform[y - y_off][x] == Tile::Empty; y_off++)
							BAN::swap(platform[y - y_off + 1][x], platform[y - y_off][x]);
			break;
		case Direction::West:
			for (size_t x = 0; x < platform.front().size(); x++)
				for (size_t y = 0; y < platform.size(); y++)
					if (platform[y][x] == Tile::RollableRock)
						for (size_t x_off = 1; x_off <= x && platform[y][x - x_off] == Tile::Empty; x_off++)
							BAN::swap(platform[y][x - x_off + 1], platform[y][x - x_off]);
			break;
		case Direction::South:
			for (size_t y = platform.size(); y > 0; y--)
				for (size_t x = 0; x < platform.front().size(); x++)
					if (platform[y - 1][x] == Tile::RollableRock)
						for (size_t y_off = 1; y + y_off - 1 < platform.size() && platform[y + y_off - 1][x] == Tile::Empty; y_off++)
							BAN::swap(platform[y + y_off - 2][x], platform[y + y_off - 1][x]);
			break;
		case Direction::East:
			for (size_t x = platform.front().size(); x > 0; x--)
				for (size_t y = 0; y < platform.size(); y++)
					if (platform[y][x - 1] == Tile::RollableRock)
						for (size_t x_off = 1; x + x_off - 1 < platform.front().size() && platform[y][x + x_off - 1] == Tile::Empty; x_off++)
							BAN::swap(platform[y][x + x_off - 2], platform[y][x + x_off - 1]);
			break;
		default:
			ASSERT_NOT_REACHED();
	}
}

i64 puzzle1(FILE* fp)
{
	auto platform = parse_platform(fp);

	tilt_platform(platform, Direction::North);

	i64 result = 0;
	for (size_t y = 0; y < platform.size(); y++)
		for (size_t x = 0; x < platform.front().size(); x++)
			if (platform[y][x] == Tile::RollableRock)
				result += platform.size() - y;
	return result;
}

struct PlatformKey
{
	BAN::Array<uint64_t, BAN::Math::div_round_up(100 * 100, 64)> data;

	bool operator==(const PlatformKey& other) const
	{
		return memcmp(data.data(), other.data.data(), data.size() * sizeof(decltype(data)::value_type)) == 0;
	}
};

struct PlatformKeyHash
{
	BAN::hash_t operator()(const PlatformKey& key) const
	{
		return key.data.back();
	}
};

i64 puzzle2(FILE* fp)
{
	auto platform = parse_platform(fp);
	ASSERT(platform.size() * platform.front().size() == 100 * 100);

	BAN::HashMap<PlatformKey, size_t, PlatformKeyHash> hit_cache;

	auto build_cache_key =
		[](const Platform& platform) -> PlatformKey
		{
			PlatformKey key;
			for (size_t i = 0; i < 100 * 100; i++)
			{
				const size_t elem = i / 64;
				const size_t bit  = i % 64;
				if (platform[i / 100][i % 100] == Tile::RollableRock)
					key.data[elem] |= 1ull << bit;
			}
			return key;
		};

	PlatformKey cycle_begin_key;
	i64 cycle_begin = -1;
	i64 cycle_length = -1;

	i64 missing_cycles = 1'000'000'000;

	for (i64 i = 0; missing_cycles > 0; i++, missing_cycles--)
	{
		auto key = build_cache_key(platform);

		if (cycle_begin == -1)
		{
			if (hit_cache.contains(key))
			{
				cycle_begin = i;
				cycle_begin_key = key;
			}
			else
			{
				MUST(hit_cache.insert(key, i));
			}
		}
		else
		{
			if (key == cycle_begin_key)
			{
				cycle_length = (i - hit_cache[key]) / 2;
				break;
			}
		}

		tilt_platform(platform, Direction::North);
		tilt_platform(platform, Direction::West);
		tilt_platform(platform, Direction::South);
		tilt_platform(platform, Direction::East);
	}

	if (cycle_length != -1)
		missing_cycles %= cycle_length;

	for (; missing_cycles > 0; missing_cycles--)
	{
		tilt_platform(platform, Direction::North);
		tilt_platform(platform, Direction::West);
		tilt_platform(platform, Direction::South);
		tilt_platform(platform, Direction::East);
	}

	i64 result = 0;
	for (size_t y = 0; y < platform.size(); y++)
		for (size_t x = 0; x < platform.front().size(); x++)
			if (platform[y][x] == Tile::RollableRock)
				result += platform.size() - y;
	return result;
}

int main(int argc, char** argv)
{
	const char* file_path = "/usr/share/aoc2023/day14_input.txt";

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
