#pragma once

#include <BAN/Math.h>
#include <BAN/Optional.h>

#include <stdint.h>

struct Position
{
	int32_t x;
	int32_t y;
};

struct Rectangle
{
	int32_t x;
	int32_t y;
	int32_t width;
	int32_t height;

	bool contains(Position position) const
	{
		if (position.x < x || position.x >= x + width)
			return false;
		if (position.y < y || position.y >= y + height)
			return false;
		return true;
	}

	BAN::Optional<Rectangle> get_overlap(Rectangle other) const
	{
		const auto min_x = BAN::Math::max(x, other.x);
		const auto min_y = BAN::Math::max(y, other.y);
		const auto max_x = BAN::Math::min(x + width, other.x + other.width);
		const auto max_y = BAN::Math::min(y + height, other.y + other.height);
		if (min_x >= max_x || min_y >= max_y)
			return {};
		return Rectangle {
			.x = min_x,
			.y = min_y,
			.width = max_x - min_x,
			.height = max_y - min_y,
		};
	}

	Rectangle get_bounding_box(Rectangle other) const
	{
		const auto min_x = BAN::Math::min(x, other.x);
		const auto min_y = BAN::Math::min(y, other.y);
		const auto max_x = BAN::Math::max(x + width, other.x + other.width);
		const auto max_y = BAN::Math::max(y + height, other.y + other.height);
		return Rectangle {
			.x = min_x,
			.y = min_y,
			.width = max_x - min_x,
			.height = max_y - min_y,
		};
	}

	bool operator==(const Rectangle& other) const
	{
		return x == other.x && y == other.y && width == other.width && height == other.height;
	}

};

struct Circle
{
	int32_t x;
	int32_t y;
	int32_t radius;

	bool contains(Position position) const
	{
		int32_t dx = position.x - x;
		int32_t dy = position.y - y;
		return dx * dx + dy * dy <= radius * radius;
	}

};
