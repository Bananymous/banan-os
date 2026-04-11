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
	int32_t min_x;
	int32_t min_y;
	int32_t max_x;
	int32_t max_y;

	int32_t width() const
	{
		return max_x - min_x;
	}

	int32_t height() const
	{
		return max_y - min_y;
	}

	int32_t area() const
	{
		return width() * height();
	}

	bool contains(Position position) const
	{
		if (position.x < min_x || position.x >= max_x)
			return false;
		if (position.y < min_y || position.y >= max_y)
			return false;
		return true;
	}

	BAN::Optional<Rectangle> get_overlap(Rectangle other) const
	{
		if (width() == 0 || height() == 0 || other.width() == 0 || other.height() == 0)
			return {};
		const auto min_x = BAN::Math::max(this->min_x, other.min_x);
		const auto min_y = BAN::Math::max(this->min_y, other.min_y);
		const auto max_x = BAN::Math::min(this->max_x, other.max_x);
		const auto max_y = BAN::Math::min(this->max_y, other.max_y);
		if (min_x >= max_x || min_y >= max_y)
			return {};
		return Rectangle {
			.min_x = min_x,
			.min_y = min_y,
			.max_x = max_x,
			.max_y = max_y,
		};
	}

	Rectangle get_bounding_box(Rectangle other) const
	{
		const auto min_x = BAN::Math::min(this->min_x, other.min_x);
		const auto min_y = BAN::Math::min(this->min_y, other.min_y);
		const auto max_x = BAN::Math::max(this->max_x, other.max_x);
		const auto max_y = BAN::Math::max(this->max_y, other.max_y);
		return Rectangle {
			.min_x = min_x,
			.min_y = min_y,
			.max_x = max_x,
			.max_y = max_y,
		};
	}

	size_t split_along_edges_of(const Rectangle& other, Rectangle out[9]) const
	{
		out[0] = *this;

		size_t vertical = 1;

		if (min_x < other.min_x && other.min_x < max_x)
		{
			auto& rect1 = out[vertical - 1];
			auto& rect2 = out[vertical - 0];
			rect2 = rect1;

			rect1.max_x = other.min_x;
			rect2.min_x = other.min_x;

			vertical++;
		}

		if (min_x < other.max_x && other.max_x < max_x)
		{
			auto& rect1 = out[vertical - 1];
			auto& rect2 = out[vertical - 0];
			rect2 = rect1;

			rect1.max_x = other.max_x;
			rect2.min_x = other.max_x;

			vertical++;
		}

		size_t horizontal = 1;

		if (min_y < other.min_y && other.min_y < max_y)
		{
			for (size_t i = 0; i < vertical; i++)
			{
				auto& rect1 = out[vertical * (horizontal - 1) + i];
				auto& rect2 = out[vertical * (horizontal - 0) + i];
				rect2 = rect1;

				rect1.max_y = other.min_y;
				rect2.min_y = other.min_y;
			}
			horizontal++;
		}

		if (min_y < other.max_y && other.max_y < max_y)
		{
			for (size_t i = 0; i < vertical; i++)
			{
				auto& rect1 = out[vertical * (horizontal - 1) + i];
				auto& rect2 = out[vertical * (horizontal - 0) + i];
				rect2 = rect1;

				rect1.max_y = other.max_y;
				rect2.min_y = other.max_y;
			}
			horizontal++;
		}

		return vertical * horizontal;
	}

	bool operator==(const Rectangle& other) const
	{
		return min_x == other.min_x && min_y == other.min_y && max_x == other.max_x && max_y == other.max_y;
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
