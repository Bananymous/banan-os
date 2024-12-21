#include <BAN/Array.h>
#include <BAN/HashMap.h>
#include <BAN/Vector.h>
#include <BAN/StringView.h>

#include <inttypes.h>
#include <stdio.h>

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

BAN::Vector<BAN::Vector<char>> parse_input(FILE* fp)
{
	BAN::Vector<BAN::Vector<char>> result;

	char buffer[8];
	while (fgets(buffer, sizeof(buffer), fp))
	{
		if (buffer[0] == '\n')
			break;
		ASSERT(buffer[4] == '\n');

		MUST(result.emplace_back());
		MUST(result.back().resize(4));
		memcpy(result.back().data(), buffer, 4);
	}

	return result;
}

struct Vec2
{
	i32 x, y;
};

static constexpr Vec2 position_numeric(char c)
{
	/*
		7 8 9
		4 5 6
		1 2 3
		. 0 A
	*/

	switch (c)
	{
		case '0': return { 1, 0 };
		case 'A': return { 2, 0 };
		case '1': return { 0, 1 };
		case '2': return { 1, 1 };
		case '3': return { 2, 1 };
		case '4': return { 0, 2 };
		case '5': return { 1, 2 };
		case '6': return { 2, 2 };
		case '7': return { 0, 3 };
		case '8': return { 1, 3 };
		case '9': return { 2, 3 };
	}

	ASSERT_NOT_REACHED();
}

static constexpr Vec2 position_directional(char c)
{
	/*
		. ^ A
		< v >
	*/

	switch (c)
	{
		case '<': return { 0, -1 };
		case 'v': return { 1, -1 };
		case '>': return { 2, -1 };
		case '^': return { 1,  0 };
		case 'A': return { 2,  0 };
	}

	ASSERT_NOT_REACHED();
}

struct Key
{
	Vec2 s;
	Vec2 e;
	u64 presses;
	u64 max_depth;

	constexpr bool operator==(const Key& other) const
	{
		return
			s.x       == other.s.x &&
			s.y       == other.s.y &&
			e.x       == other.e.x &&
			e.y       == other.e.y &&
			presses   == other.presses &&
			max_depth == other.max_depth;
	}
};

struct KeyHash
{
	constexpr BAN::hash_t operator()(const Key& key) const
	{
		return
			BAN::hash<i32>()(key.s.x) ^
			BAN::hash<i32>()(key.s.y) ^
			BAN::hash<i32>()(key.e.x) ^
			BAN::hash<i32>()(key.e.y) ^
			BAN::hash<u64>()(key.presses) ^
			BAN::hash<u64>()(key.max_depth);
	}
};

static BAN::HashMap<Key, u64, KeyHash> s_cache;

static constexpr u64 recurse(Vec2 s, Vec2 e, u64 presses, u32 max_depth)
{
	using BAN::Math::abs;

	if (max_depth == 0)
		return presses;

	const auto cache_key = Key {
		s, e, presses, max_depth
	};

	auto it = s_cache.find(cache_key);
	if (it != s_cache.end())
		return it->value;

	const auto diff = Vec2 {
		.x = e.x - s.x,
		.y = e.y - s.y,
	};

	u64 result = 0;

	if (diff.x == 0)
	{
		const auto t_pos = position_directional(diff.y > 0 ? '^' : 'v');
		const auto a_pos = position_directional('A');
		result += recurse(a_pos, t_pos, abs(diff.y), max_depth - 1);
		result += recurse(t_pos, a_pos, presses,     max_depth - 1);
	}
	else if (diff.y == 0)
	{
		const auto t_pos = position_directional(diff.x > 0 ? '>' : '<');
		const auto a_pos = position_directional('A');
		result += recurse(a_pos, t_pos, abs(diff.x), max_depth - 1);
		result += recurse(t_pos, a_pos, presses,     max_depth - 1);
	}
	else if (s.y == 0 && e.x == 0)
	{
		const auto t1_pos = position_directional(diff.y > 0 ? '^' : 'v');
		const auto t2_pos = position_directional(diff.x > 0 ? '>' : '<');
		const auto a_pos = position_directional('A');
		result += recurse(a_pos,  t1_pos, abs(diff.y), max_depth - 1);
		result += recurse(t1_pos, t2_pos, abs(diff.x), max_depth - 1);
		result += recurse(t2_pos, a_pos,  presses,     max_depth - 1);
	}
	else if ((s.x == 0 && e.y == 0) || diff.x < 0)
	{
		const auto t1_pos = position_directional(diff.x > 0 ? '>' : '<');
		const auto t2_pos = position_directional(diff.y > 0 ? '^' : 'v');
		const auto a_pos = position_directional('A');
		result += recurse(a_pos,  t1_pos, abs(diff.x), max_depth - 1);
		result += recurse(t1_pos, t2_pos, abs(diff.y), max_depth - 1);
		result += recurse(t2_pos, a_pos,  presses,     max_depth - 1);
	}
	else
	{
		const auto t1_pos = position_directional(diff.y > 0 ? '^' : 'v');
		const auto t2_pos = position_directional(diff.x > 0 ? '>' : '<');
		const auto a_pos = position_directional('A');
		result += recurse(a_pos,  t1_pos, abs(diff.y), max_depth - 1);
		result += recurse(t1_pos, t2_pos, abs(diff.x), max_depth - 1);
		result += recurse(t2_pos, a_pos,  presses,     max_depth - 1);
	}

	MUST(s_cache.insert(cache_key, result));

	return result;
}

i64 part1(FILE* fp)
{
	auto input = parse_input(fp);

	u64 result = 0;
	for (auto code : input)
	{
		u64 length = 0;
		for (usize i = 0; i < code.size(); i++)
		{
			const auto s_pos = position_numeric(i ? code[i - 1] : 'A');
			const auto e_pos = position_numeric(code[i]);
			length += recurse(s_pos, e_pos, 1, 3);
		}
		result += length * atoi(code.data());
	}
	return result;
}

i64 part2(FILE* fp)
{
	auto input = parse_input(fp);

	u64 result = 0;
	for (auto code : input)
	{
		u64 length = 0;
		for (usize i = 0; i < code.size(); i++)
		{
			const auto s_pos = position_numeric(i ? code[i - 1] : 'A');
			const auto e_pos = position_numeric(code[i]);
			length += recurse(s_pos, e_pos, 1, 26);
		}
		result += length * atoi(code.data());
	}
	return result;
}

int main(int argc, char** argv)
{
	const char* file_path = "/usr/share/aoc2024/day21_input.txt";

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
