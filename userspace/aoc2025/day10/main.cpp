#include <BAN/String.h>
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

struct Machine
{
	u32 indicator;
	BAN::Vector<u32> buttons;
	BAN::Vector<u32> joltages;
};

static BAN::Vector<Machine> parse_machines(FILE* fp)
{
	BAN::Vector<Machine> machines;

	BAN::String line;
	char buffer[128];
	while (fgets(buffer, sizeof(buffer), fp))
	{
		MUST(line.append(buffer));
		if (line.back() != '\n')
			continue;
		if (line.front() != '[')
			break;

		Machine machine {};

		size_t i = 1;
		while (line[i] != ']')
		{
			if (line[i] == '#')
				machine.indicator |= 1 << (i - 1);
			i++;
		}

		i += 2;

		while (line[i++] == '(')
		{
			u32 button = 0;

			while (line[i] != ')')
			{
				if (line[i] == ',')
					i++;

				u32 index = 0;
				while (isdigit(line[i]))
					index = (index * 10) + (line[i++] - '0');

				button |= 1 << index;
			}

			MUST(machine.buttons.push_back(button));

			i += 2;
		}

		i--;

		ASSERT(line[i] == '{');
		while (line[i++] != '}')
		{
			u32 joltage = 0;
			while (isdigit(line[i]))
				joltage = (joltage * 10) + (line[i++] - '0');
			MUST(machine.joltages.push_back(joltage));
		}

		MUST(machines.push_back(BAN::move(machine)));

		line.clear();
	}

	return machines;
}

i64 part1(FILE* fp)
{
	static constexpr bool (*works_with_n_buttons)(const Machine&, size_t, size_t, u32) =
		[](const Machine& machine, size_t buttons, size_t index, u32 state) -> bool
		{
			if (buttons == 0)
				return state == machine.indicator;
			for (size_t i = index; i < machine.buttons.size(); i++)
				if (works_with_n_buttons(machine, buttons - 1, i + 1, state ^ machine.buttons[i]))
					return true;
			return false;
		};

	i64 result = 0;

	auto machines = parse_machines(fp);
	for (const auto& machine : machines)
	{
		for (size_t i = 1; i < machine.buttons.size(); i++)
		{
			if (!works_with_n_buttons(machine, i, 0, 0))
				continue;
			result += i;
			break;
		}
	}

	return result;
}

// based on https://www.reddit.com/r/adventofcode/comments/1pk87hl/2025_day_10_part_2_bifurcate_your_way_to_victory/

static u32 minimum_presses(const BAN::Vector<BAN::Vector<u32>>& possible_indicators, const BAN::Vector<u32>& buttons, const BAN::Vector<u32>& joltages)
{
	bool all_zeroes = true;
	for (u32 joltage : joltages)
		if (joltage != 0)
			all_zeroes = false;
	if (all_zeroes)
		return 0;

	u32 indicator = 0;
	for (size_t i = 0; i < joltages.size(); i++)
		if (joltages[i] % 2)
			indicator |= 1 << i;

	u32 result = 1'000'000;

	for (u32 button_mask : possible_indicators[indicator])
	{
		size_t button_count = 0;
		BAN::Vector<u32> new_joltages = joltages;
		for (size_t i = 0; i < buttons.size(); i++)
		{
			if (!(button_mask & (1 << i)))
				continue;
			button_count++;
			for (size_t bit = 0; bit < new_joltages.size(); bit++)
			{
				if (!(buttons[i] & (1 << bit)))
					continue;
				if (new_joltages[bit] == 0)
					goto unusable;
				new_joltages[bit]--;
			}
		}

		for (u32& joltage : new_joltages)
			joltage /= 2;
		result = BAN::Math::min<u32>(result, button_count + 2 * minimum_presses(possible_indicators, buttons, new_joltages));

	unusable:
		(void)0;
	}

	return result;
}

i64 part2(FILE* fp)
{
	i64 result = 0;

	auto machines = parse_machines(fp);
	for (const auto& machine : machines)
	{
		const size_t bits = machine.joltages.size();

		BAN::Vector<BAN::Vector<u32>> possible_indicators;
		MUST(possible_indicators.resize(1u << bits));
		for (size_t button_mask = 0; button_mask < (1u << machine.buttons.size()); button_mask++)
		{
			u32 indicator = 0;
			for (size_t button = 0; button < machine.buttons.size(); button++)
				if (button_mask & (1 << button))
					indicator ^= machine.buttons[button];
			MUST(possible_indicators[indicator].push_back(button_mask));
		}

		result += minimum_presses(possible_indicators, machine.buttons, machine.joltages);
	}

	return result;
}

int main(int argc, char** argv)
{
	const char* file_path = "/usr/share/aoc2025/day10_input.txt";

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
