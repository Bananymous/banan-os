#include <BAN/HashMap.h>
#include <BAN/HashSet.h>
#include <BAN/Sort.h>
#include <BAN/String.h>

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

struct ParseInputResult
{
	BAN::HashMap<u32, BAN::Vector<u32>> connections;
	BAN::HashSet<u32> all_nodes;
};

static ParseInputResult parse_input(FILE* fp)
{
	BAN::HashMap<u32, BAN::Vector<u32>> connections;
	BAN::HashSet<u32> all_nodes;

	char buffer[16];
	while (fgets(buffer, sizeof(buffer), fp))
	{
		ASSERT(buffer[2] == '-' && buffer[5] == '\n');
		const u16 lhs = (buffer[0] << 8) | buffer[1];
		const u16 rhs = (buffer[3] << 8) | buffer[4];

		const u16 min = BAN::Math::min(lhs, rhs);
		const u16 max = BAN::Math::max(lhs, rhs);

		auto it = connections.find(min);
		if (it == connections.end())
			it = MUST(connections.emplace(min));
		MUST(it->value.push_back(max));

		MUST(all_nodes.insert(lhs));
		MUST(all_nodes.insert(rhs));
	}

	return {
		.connections = BAN::move(connections),
		.all_nodes = BAN::move(all_nodes),
	};
}

i64 part1(FILE* fp)
{
	auto [connections, _] = parse_input(fp);

	u64 result = 0;

	for (const auto& [con1, dst1] : connections)
	{
		for (const u16 con2 : dst1)
		{
			auto it = connections.find(con2);
			if (it == connections.end())
				continue;
			for (const u16 con3 : it->value)
			{
				if (!dst1.contains(con3))
					continue;
				result +=
					((char)(con1 >> 8) == 't') ||
					((char)(con2 >> 8) == 't') ||
					((char)(con3 >> 8) == 't');
			}
		}
	}

	return result;
}

static bool is_connected_to_all_nodes(const BAN::HashMap<u32, BAN::Vector<u32>>& connections, const BAN::Vector<u32>& nodes, u32 test)
{
	if (nodes.empty())
		return true;

	auto it = connections.find(test);
	if (it == connections.end())
		return false;
	const auto& test_con = it->value;

	usize found = 0;
	for (usize i = 0; i < test_con.size() && found < nodes.size(); i++)
		if (test_con[i] == nodes[found])
			found++;
	return found == nodes.size();
}

static BAN::Vector<u32> maximum_clique(const BAN::HashMap<u32, BAN::Vector<u32>>& connections, const BAN::Vector<u32>& nodes, BAN::Vector<u32>&& current, usize idx)
{
	if (idx == nodes.size())
		return current;

	BAN::Vector<u32> result;

	const u32 check = nodes[nodes.size() - idx - 1];
	if (is_connected_to_all_nodes(connections, current, check))
	{
		BAN::Vector<u32> temp;
		MUST(temp.resize(current.size() + 1));
		for (usize i = 0; i < current.size(); i++)
			temp[i + 1] = current[i];
		temp[0] = check;
		result = maximum_clique(connections, nodes, BAN::move(temp), idx + 1);
	}

	auto temp = maximum_clique(connections, nodes, BAN::move(current), idx + 1);
	if (temp.size() > result.size())
		result = BAN::move(temp);

	return result;
}

BAN::String part2(FILE* fp)
{
	auto [connections, set_all_nodes] = parse_input(fp);

	for (auto& [_, conn] : connections)
		BAN::sort::sort(conn.begin(), conn.end());

	BAN::Vector<u32> all_nodes;
	MUST(all_nodes.reserve(set_all_nodes.size()));
	for (const u32 node : set_all_nodes)
		MUST(all_nodes.push_back(node));
	BAN::sort::sort(all_nodes.begin(), all_nodes.end());

	auto clique = maximum_clique(connections, all_nodes, {}, 0);

	BAN::String result;
	for (u32 node : clique)
		MUST(result.append(MUST(BAN::String::formatted("{}{},", (char)(node >> 8), (char)node))));
	if (!result.empty())
		result.pop_back();
	return result;
}

int main(int argc, char** argv)
{
	const char* file_path = "/usr/share/aoc2024/day23_input.txt";

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
