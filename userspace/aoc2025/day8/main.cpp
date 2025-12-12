#include <BAN/HashSet.h>
#include <BAN/Heap.h>
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

struct Point
{
	i64 x, y, z;

	constexpr bool operator==(const Point& other) const
	{
		return x == other.x && y == other.y && z == other.z;
	}
};

static BAN::Vector<Point> parse_points(FILE* fp)
{
	BAN::Vector<Point> points;

	i64 x, y, z;
	while (fscanf(fp, "%" SCNi64 ",%" SCNi64 ",%" SCNi64, &x, &y, &z) == 3)
		MUST(points.emplace_back(x, y, z));

	return points;
}

struct PointPair
{
	Point a, b;

	constexpr i64 dist() const
	{
		return (a.x - b.x) * (a.x - b.x)
		     + (a.y - b.y) * (a.y - b.y)
		     + (a.z - b.z) * (a.z - b.z);
	}

	constexpr bool operator>(const PointPair& other) const
	{
		return dist() > other.dist();
	}
};

static BAN::Vector<PointPair> build_point_pair_heap(const BAN::Vector<Point>& points)
{
	BAN::Vector<PointPair> pairs;
	MUST(pairs.reserve(points.size() * (points.size() + 1) / 2));

	for (size_t i = 0; i < points.size(); i++)
		for (size_t j = i + 1; j < points.size(); j++)
			MUST(pairs.push_back({ points[i], points[j] }));

	BAN::make_heap(pairs.begin(), pairs.end(), BAN::greater<PointPair> {});

	return pairs;
}

using Circuit = BAN::Vector<Point>;

static BAN::Vector<Circuit> build_circuits(const BAN::Vector<Point>& points)
{
	BAN::Vector<Circuit> circuits;
	MUST(circuits.reserve(points.size()));

	for (size_t i = 0; i < points.size(); i++)
		MUST(circuits.emplace_back(1, points[i]));

	return circuits;
}

static void combine_circuits_with_points(BAN::Vector<Circuit>& circuits, const Point& a, const Point& b)
{
	size_t a_index = SIZE_MAX;
	size_t b_index = SIZE_MAX;

	for (size_t i = 0; i < circuits.size(); i++)
	{
		if (circuits[i].contains(a))
			a_index = i;
		if (circuits[i].contains(b))
			b_index = i;
	}

	if (a_index == b_index)
		return;

	MUST(circuits[a_index].reserve(circuits[a_index].size() + circuits[b_index].size()));
	for (auto point : circuits[b_index])
		MUST(circuits[a_index].push_back(point));

	circuits.remove(b_index);
}

i64 part1(FILE* fp)
{
	auto points = parse_points(fp);
	auto pairs = build_point_pair_heap(points);
	auto circuits = build_circuits(points);

	for (size_t i = 0; i < 1000; i++)
	{
		const auto [a, b] = pairs.front();
		BAN::pop_heap(pairs.begin(), pairs.end() - i, BAN::greater<PointPair> {});

		combine_circuits_with_points(circuits, a, b);
	}

	const auto size_comp =
		[](const auto& a, const auto& b)
		{
			return a.size() < b.size();
		};

	BAN::make_heap(circuits.begin(), circuits.end(), size_comp);

	i64 result = 1;

	for (size_t i = 0; i < 3; i++)
	{
		result *= circuits.front().size();
		BAN::pop_heap(circuits.begin(), circuits.end() - i, size_comp);
	}

	return result;
}

i64 part2(FILE* fp)
{
	auto points = parse_points(fp);
	auto pairs = build_point_pair_heap(points);
	auto circuits = build_circuits(points);

	for (size_t i = 0; i < pairs.size(); i++)
	{
		const auto [a, b] = pairs.front();
		BAN::pop_heap(pairs.begin(), pairs.end() - i, BAN::greater<PointPair> {});

		combine_circuits_with_points(circuits, a, b);
		if (circuits.size() == 1)
			return a.x * b.x;
	}

	ASSERT_NOT_REACHED();
}

int main(int argc, char** argv)
{
	const char* file_path = "/usr/share/aoc2025/day8_input.txt";

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
