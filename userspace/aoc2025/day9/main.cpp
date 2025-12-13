#include <BAN/HashMap.h>
#include <BAN/Sort.h>
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
	i64 x, y;
};

static BAN::Vector<Point> parse_points(FILE* fp)
{
	BAN::Vector<Point> points;

	i64 x, y;
	while (fscanf(fp, "%" SCNi64 ",%" SCNi64, &x, &y) == 2)
		MUST(points.emplace_back(x, y));

	return points;
}

i64 part1(FILE* fp)
{
	auto points = parse_points(fp);

	i64 result = 0;

	for (size_t i = 0; i < points.size(); i++)
	{
		for (size_t j = i + 1; j < points.size(); j++)
		{
			const i64 w = BAN::Math::abs(points[i].x - points[j].x) + 1;
			const i64 h = BAN::Math::abs(points[i].y - points[j].y) + 1;
			result = BAN::Math::max(result, w * h);
		}
	}

	return result;
}

static BAN::Vector<i64> compress_coordinates(BAN::Vector<Point>& points)
{
	BAN::HashMap<i64, i64> coord_map;
	MUST(coord_map.reserve(points.size() * 4));
	for (size_t i = 0; i < points.size(); i++)
	{
		MUST(coord_map.emplace_or_assign(points[i].x));
		MUST(coord_map.emplace_or_assign(points[i].y));
	}

	BAN::Vector<i64> coords;
	MUST(coords.reserve(coord_map.size()));
	for (auto [coord, _] : coord_map)
		MUST(coords.push_back(coord));
	BAN::sort::sort(coords.begin(), coords.end());

	for (size_t i = 0; i < coords.size(); i++)
		coord_map[coords[i]] = i;

	for (auto& point : points)
	{
		point.x = coord_map[point.x];
		point.y = coord_map[point.y];
	}

	return coords;
}

i64 part2(FILE* fp)
{
	auto points = parse_points(fp);
	auto index2coord = compress_coordinates(points);

	const size_t grid_size = index2coord.size();

	BAN::Vector<bool> grid;
	MUST(grid.resize(grid_size * grid_size, false));

	for (size_t i = 0; i < points.size(); i++)
	{
		const auto curr = points[i];
		const auto next = points[(i + 1) % points.size()];
		ASSERT((curr.x == next.x) != (curr.y == next.y));

		const i64 minx = BAN::Math::min(curr.x, next.x);
		const i64 maxx = BAN::Math::max(curr.x, next.x);
		const i64 miny = BAN::Math::min(curr.y, next.y);
		const i64 maxy = BAN::Math::max(curr.y, next.y);

		const i64 dx = (minx != maxx);
		const i64 dy = (miny != maxy);

		for (i64 x = minx, y = miny; x <= maxx && y <= maxy; x += dx, y += dy)
			grid[y * grid_size + x] = true;
	}

	for (size_t y = 0; y < grid_size; y++)
	{
		bool is_inside = false;
		for (size_t x = 0; x < grid_size; x++)
		{
			if (!grid[y * grid_size + x])
				grid[y * grid_size + x] = is_inside;
			else if (x + 1 >= grid_size || !grid[y * grid_size + x + 1])
				is_inside = !is_inside;
			else
			{
				const bool from_below1 = (y + 1 < grid_size) && grid[(y + 1) * grid_size + x];
				while (x + 1 < grid_size && grid[y * grid_size + x + 1])
					x++;
				const bool from_below2 = (y + 1 < grid_size) && grid[(y + 1) * grid_size + x];

				if (from_below1 != from_below2)
					is_inside = !is_inside;
			}
		}
	}

	const auto is_usable_area =
		[&grid, grid_size](const Point& a, const Point& b) -> bool
		{
			const i64 minx = BAN::Math::min(a.x, b.x);
			const i64 maxx = BAN::Math::max(a.x, b.x);
			const i64 miny = BAN::Math::min(a.y, b.y);
			const i64 maxy = BAN::Math::max(a.y, b.y);
			for (i64 x = minx; x <= maxx; x++)
				if (!grid[miny * grid_size + x] || !grid[maxy * grid_size + x])
					return false;
			for (i64 y = miny; y <= maxy; y++)
				if (!grid[y * grid_size + minx] || !grid[y * grid_size + maxy])
					return false;
			return true;
		};

	i64 result = 0;

	for (size_t i = 0; i < points.size(); i++)
	{
		for (size_t j = i + 1; j < points.size(); j++)
		{
			const auto& a = points[i];
			const auto& b = points[j];
			if (!is_usable_area(a, b))
				continue;

			const i64 w = BAN::Math::abs(index2coord[a.x] - index2coord[b.x]) + 1;
			const i64 h = BAN::Math::abs(index2coord[a.y] - index2coord[b.y]) + 1;
			result = BAN::Math::max(result, w * h);
		}
	}

	return result;
}

int main(int argc, char** argv)
{
	const char* file_path = "/usr/share/aoc2025/day9_input.txt";

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
