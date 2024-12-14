#include <BAN/Assert.h>
#include <BAN/Math.h>

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
using usize = size_t;

struct vec_t { i64 x, y; };

u64 fewest_tokens(vec_t a, vec_t b, vec_t p)
{
	ASSERT(a.x * b.y != b.x * a.y);

	// n * a.x + m * b.x = p.x  <==>  m = (p.x - n * a.x) / b.x
	// n * a.y + m * b.y = p.y

	// n * a.y + ((p.x - n * a.x) / b.x) * b.y = p.y
	// => n * a.y + (p.x / b.x - n * a.x / b.x) * b.y = p.y
	// => n * a.y + p.x / b.x * b.y - n * a.x / b.x * b.y = p.y
	// => n * a.y - n * a.x / b.x * b.y = p.y - p.x / b.x * b.y
	// => n * a.y * b.x - n * a.x * b.y = p.y * b.x - p.x * b.y
	// => n * (a.y * b.x - a.x * b.y) = p.y * b.x - p.x * b.y
	// => n = (p.y * b.x - p.x * b.y) / (a.y * b.x - a.x * b.y)

	const i64 n_num = p.y * b.x - p.x * b.y;
	const i64 n_den = a.y * b.x - a.x * b.y;
	if (n_num % n_den)
		return 0;
	const i64 n = n_num / n_den;

	const i64 m_num = p.x - n * a.x;
	const i64 m_den = b.x;
	if (m_num % m_den)
		return 0;
	const i64 m = m_num / m_den;

	return 3 * n + m;
}

i64 part1(FILE* fp)
{
	i64 result = 0;

	for (;;)
	{
		constexpr const char* format =
			"Button A: X+%" SCNd64 ", Y+%" SCNd64 "\n"
			"Button B: X+%" SCNd64 ", Y+%" SCNd64 "\n"
			"Prize: X=%" SCNd64 ", Y=%" SCNd64 "\n";

		vec_t a, b, prize;
		if (fscanf(fp, format, &a.x, &a.y, &b.x, &b.y, &prize.x, &prize.y) != 6)
			break;

		result += fewest_tokens(a, b, prize);
	}

	return result;
}

i64 part2(FILE* fp)
{
	i64 result = 0;

	for (;;)
	{
		constexpr const char* format =
			"Button A: X+%" SCNu64 ", Y+%" SCNu64 "\n"
			"Button B: X+%" SCNu64 ", Y+%" SCNu64 "\n"
			"Prize: X=%" SCNu64 ", Y=%" SCNu64 "\n";

		vec_t a, b, prize;
		if (fscanf(fp, format, &a.x, &a.y, &b.x, &b.y, &prize.x, &prize.y) != 6)
			break;

		prize.x += 10000000000000;
		prize.y += 10000000000000;
		result += fewest_tokens(a, b, prize);
	}

	return result;
}

int main(int argc, char** argv)
{
	const char* file_path = "/usr/share/aoc2024/day13_input.txt";

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
