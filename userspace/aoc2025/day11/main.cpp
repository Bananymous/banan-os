#include <BAN/HashMap.h>
#include <BAN/StringView.h>
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

struct Device
{
	u32 distance { 0 };
	BAN::Vector<u32> outputs;
};

static inline constexpr u32 name_to_u32(BAN::StringView name)
{
	ASSERT(name.size() == 3);
	return name[0] | (name[1] << 8) | (name[2] << 16);
}

static BAN::HashMap<u32, Device> parse_devices(FILE* fp, const char* first_node)
{
	BAN::HashMap<u32, Device> result;

	char buffer[128];
	while (fgets(buffer, sizeof(buffer), fp))
	{
		auto buffer_sv = BAN::StringView(buffer);
		ASSERT(buffer_sv.back() == '\n');
		buffer_sv = buffer_sv.substring(0, buffer_sv.size() - 1);

		const u32 device = name_to_u32(buffer_sv.substring(0, 3));

		auto it = result.find(device);
		if (it == result.end())
			it = MUST(result.emplace(device));

		const auto outputs = MUST(buffer_sv.substring(5).split(' '));
		for (auto name : outputs)
			MUST(it->value.outputs.push_back(name_to_u32(name)));
	}

	MUST(result.emplace(name_to_u32("out")));

	BAN::Vector<u32> to_update;
	MUST(to_update.push_back(name_to_u32(first_node)));

	u32 distance = 1;
	while (!to_update.empty())
	{
		BAN::Vector<u32> next_update;

		for (u32 name : to_update)
		{
			auto& device = result[name];
			if (device.distance >= distance)
				continue;
			for (u32 output : device.outputs)
				MUST(next_update.push_back(output));
			device.distance = distance;
		}

		to_update = BAN::move(next_update);
		distance++;
	}

	return result;
}

static i64 count_paths(const BAN::HashMap<u32, Device>& devices, BAN::HashMap<u32, u32>& cache, u32 current, u32 destination)
{
	if (current == destination)
		return 1;
	if (auto it = cache.find(current); it != cache.end())
		return it->value;
	if (devices[current].distance >= devices[destination].distance)
		return 0;

	i64 result = 0;

	const auto& outputs = devices[current].outputs;
	for (u32 output : outputs)
		result += count_paths(devices, cache, output, destination);

	MUST(cache.emplace(current, result));

	return result;
}

i64 part1(FILE* fp)
{
	auto devices = parse_devices(fp, "you");

	BAN::HashMap<u32, u32> cache;
	return count_paths(devices, cache, name_to_u32("you"), name_to_u32("out"));
}

i64 part2(FILE* fp)
{
	auto devices = parse_devices(fp, "svr");

	const bool fft_before = devices[name_to_u32("fft")].distance < devices[name_to_u32("dac")].distance;
	const u32 real_path[] {
		name_to_u32("svr"),
		fft_before ? name_to_u32("fft") : name_to_u32("dac"),
		fft_before ? name_to_u32("dac") : name_to_u32("fft"),
		name_to_u32("out"),
	};

	i64 result = 1;

	for (size_t i = 1; i < sizeof(real_path) / sizeof(*real_path); i++)
	{
		BAN::HashMap<u32, u32> cache;
		result *= count_paths(devices, cache, real_path[i - 1], real_path[i]);
	}

	return result;
}

int main(int argc, char** argv)
{
	const char* file_path = "/usr/share/aoc2025/day11_input.txt";

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
