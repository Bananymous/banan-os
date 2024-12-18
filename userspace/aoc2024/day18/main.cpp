#include <BAN/Array.h>
#include <BAN/String.h>
#include <BAN/Vector.h>

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

template<typename T>
struct Vec2
{
	T x, y;
};

template<usize W, usize H>
static BAN::Optional<usize> steps_to_finnish(BAN::Array<BAN::Array<bool, W>, H>& map)
{
	BAN::Vector<Vec2<usize>> pending;
	MUST(pending.push_back({ 0, 0 }));

	for (usize step = 0;; step++)
	{
		BAN::Vector<Vec2<usize>> next;
		for (auto pos : pending)
		{
			if (map[pos.y][pos.x])
				continue;
			map[pos.y][pos.x] = true;

			if (pos.x == W - 1 && pos.y == H - 1)
				return step;

			if (pos.y > 0     && !map[pos.y - 1][pos.x    ]) MUST(next.push_back({ pos.x,     pos.y - 1 }));
			if (pos.x > 0     && !map[pos.y    ][pos.x - 1]) MUST(next.push_back({ pos.x - 1, pos.y     }));
			if (pos.y < H - 1 && !map[pos.y + 1][pos.x    ]) MUST(next.push_back({ pos.x,     pos.y + 1 }));
			if (pos.x < W - 1 && !map[pos.y    ][pos.x + 1]) MUST(next.push_back({ pos.x + 1, pos.y     }));
		}

		if (next.empty())
			break;
		pending = BAN::move(next);
	}

	return {};
}

i64 part1(FILE* fp)
{
	BAN::Array<BAN::Array<bool, 71>, 71> map(false);
	for (usize i = 0; i < 1024; i++)
	{
		usize x, y;
		ASSERT(fscanf(fp, "%zu,%zu\n", &x, &y) == 2);
		map[y][x] = true;
	}

	return steps_to_finnish(map).value();
}

BAN::String part2(FILE* fp)
{
	BAN::Vector<Vec2<usize>> bytes;
	for (;;)
	{
		usize x, y;
		if (fscanf(fp, "%zu,%zu\n", &x, &y) != 2)
			break;
		MUST(bytes.push_back({ x, y }));
	}

	usize l = 0;
	usize r = bytes.size() - 1;

	while (l <= r)
	{
		const usize mid = l + (r - l) / 2;

		BAN::Array<BAN::Array<bool, 71>, 71> map(false);
		for (usize i = 0; i <= mid; i++)
			map[bytes[i].y][bytes[i].x] = true;

		if (steps_to_finnish(map).has_value())
			l = mid + 1;
		else if (l == mid)
			return MUST(BAN::String::formatted("{},{}", bytes[mid].x, bytes[mid].y));
		else
			r = mid;
	}

	ASSERT_NOT_REACHED();
}

int main(int argc, char** argv)
{
	const char* file_path = "/usr/share/aoc2024/day18_input.txt";

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

	printf("part2: %s\n", part2(fp).data());

	fclose(fp);
}
