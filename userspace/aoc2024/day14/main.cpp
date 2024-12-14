#include <BAN/Array.h>
#include <BAN/Vector.h>

#include <fcntl.h>
#include <inttypes.h>
#include <stdio.h>
#include <sys/framebuffer.h>
#include <sys/mman.h>
#include <termios.h>

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

i64 part1(FILE* fp)
{
	i32 quadrants[4] {};

	for (;;)
	{
		i32 px, py, vx, vy;
		if (fscanf(fp, "p=%" SCNd32 ",%" SCNd32 " v=%" SCNd32 ",%" SCNd32 "\n", &px, &py, &vx, &vy) != 4)
			break;

		const i32 tx = (((px + 100 * vx) % 101) + 101) % 101;
		const i32 ty = (((py + 100 * vy) % 103) + 103) % 103;

		if (tx < 50 && ty < 51)
			quadrants[0]++;
		if (tx > 50 && ty < 51)
			quadrants[1]++;
		if (tx < 50 && ty > 51)
			quadrants[2]++;
		if (tx > 50 && ty > 51)
			quadrants[3]++;
	}

	return quadrants[0] * quadrants[1] * quadrants[2] * quadrants[3];
}

struct Robot
{
	i32 px, py;
	i32 vx, vy;
};

usize largest_area(const BAN::Vector<Robot>& robots)
{
	BAN::Array<bool, 101 * 103> robot_map(false);
	for (const auto& robot : robots)
		robot_map[robot.py * 101 + robot.px] = true;

	BAN::Array<bool, 101 * 103> checked(false);

	usize result = 0;
	for (i32 y = 0; y < 103; y++)
	{
		for (i32 x = 0; x < 101; x++)
		{
			if (checked[y * 101 + x])
				continue;
			if (!robot_map[y * 101 + x])
				continue;

			struct Position { i32 x, y; };
			BAN::Vector<Position> to_check;
			MUST(to_check.reserve(101 * 103));

			MUST(to_check.emplace_back(x, y));

			usize area = 0;
			while (!to_check.empty())
			{
				const auto pos = to_check.back();
				to_check.pop_back();

				if (checked[pos.y * 101 + pos.x])
					continue;
				checked[pos.y * 101 + pos.x] = true;

				if (!robot_map[pos.y * 101 + pos.x])
					continue;

				area++;

				if (pos.x - 1 >=  0) MUST(to_check.emplace_back(pos.x - 1, pos.y    ));
				if (pos.x + 1 < 101) MUST(to_check.emplace_back(pos.x + 1, pos.y    ));
				if (pos.y - 1 >=  0) MUST(to_check.emplace_back(pos.x,     pos.y - 1));
				if (pos.y + 1 < 103) MUST(to_check.emplace_back(pos.x,     pos.y + 1));
			}

			result = BAN::Math::max(result, area);
		}
	}

	return result;
}

i64 part2(FILE* fp)
{
	BAN::Vector<Robot> robots;

	for (;;)
	{
		i32 px, py, vx, vy;
		if (fscanf(fp, "p=%" SCNd32 ",%" SCNd32 " v=%" SCNd32 ",%" SCNd32 "\n", &px, &py, &vx, &vy) != 4)
			break;
		MUST(robots.emplace_back(px, py, vx, vy));
	}

	for (i64 frame = 0;; frame++)
	{
		if (largest_area(robots) >= robots.size() / 4)
			return frame;
		for (auto& robot : robots)
		{
			robot.px = (((robot.px + robot.vx) % 101) + 101) % 101;
			robot.py = (((robot.py + robot.vy) % 103) + 103) % 103;
		}
	}
}

int main(int argc, char** argv)
{
	const char* file_path = "/usr/share/aoc2024/day14_input.txt";

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
