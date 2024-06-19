#include <BAN/HashMap.h>
#include <BAN/HashSet.h>
#include <BAN/Sort.h>
#include <BAN/String.h>
#include <BAN/Vector.h>

#include <ctype.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>

using i8  = int8_t;
using i16 = int16_t;
using i32 = int32_t;
using i64 = int64_t;

using u8  = uint8_t;
using u16 = uint16_t;
using u32 = uint32_t;
using u64 = uint64_t;

struct Component
{
	Component(BAN::String name)
		: name(BAN::move(name))
	{}
	BAN::String name;
	BAN::Vector<BAN::String> connections;
};

BAN::HashMap<BAN::String, Component> parse_components(FILE* fp)
{
	BAN::HashMap<BAN::String, Component> components;

	char buffer[128];
	while (fgets(buffer, sizeof(buffer), fp))
	{
		BAN::StringView line(buffer);
		ASSERT(line.back() == '\n');
		line = line.substring(0, line.size() - 1);
		if (line.empty())
			break;

		auto parts = MUST(line.split(' '));
		ASSERT(parts.size() >= 2);

		ASSERT(parts.front().back() == ':');
		parts.front() = parts.front().substring(0, parts.front().size() - 1);

		if (!components.contains(parts.front()))
			MUST(components.emplace(parts.front(), parts.front()));

		for (size_t i = 1; i < parts.size(); i++)
		{
			MUST(components[parts.front()].connections.emplace_back(parts[i]));

			if (!components.contains(parts[i]))
				MUST(components.emplace(parts[i], parts[i]));
			MUST(components[parts[i]].connections.emplace_back(parts.front()));
		}
	}

	return components;
}

BAN::String connection_key(const BAN::String& a, const BAN::String& b)
{
	auto comp =
		[](const auto& a, const auto& b)
		{
			ASSERT(a.size() == b.size());
			for (size_t i = 0; i < a.size(); i++)
				if (a[i] != b[i])
					return a[i] < b[i];
			ASSERT_NOT_REACHED();
		};

	const auto& s1 = comp(a, b) ? a : b;
	const auto& s2 = comp(a, b) ? b : a;

	auto key = s1;
	MUST(key.append(s2));
	return key;
}

size_t graph_size(const BAN::HashMap<BAN::String, Component>& graph, const BAN::String& start, const BAN::HashSet<BAN::String>& removed)
{
	BAN::HashSet<BAN::String> visited;
	BAN::HashSet<BAN::String> pending;
	MUST(pending.insert(start));

	while (!pending.empty())
	{
		auto current = *pending.begin();
		pending.remove(current);

		MUST(visited.insert(current));

		const auto& targets = graph[current].connections;
		for (const auto& target : targets)
		{
			if (removed.contains(connection_key(current, target)))
				continue;
			if (visited.contains(target))
				continue;
			MUST(pending.insert(target));
			MUST(visited.insert(target));
		}
	}

	return visited.size();
}

BAN::Vector<BAN::String> find_shortest_path(const BAN::String& start, const BAN::String& end, const BAN::HashMap<BAN::String, Component>& graph)
{
	BAN::HashMap<BAN::String, BAN::Vector<BAN::String>> paths;
	MUST(paths.insert(start, {}));
	MUST(paths[start].push_back(start));

	BAN::HashSet<BAN::String> visited;
	BAN::HashSet<BAN::String> pending;
	MUST(pending.insert(start));

	while (!pending.empty())
	{
		BAN::HashSet<BAN::String> next_pending;

		while (!pending.empty())
		{
			auto current = *pending.begin();
			pending.remove(current);
			MUST(visited.insert(current));

			if (current == end)
				return paths[current];

			const auto& targets = graph[current].connections;
			for (const auto& target : targets)
			{
				if (visited.contains(target))
					continue;
				if (pending.contains(target))
					continue;
				if (next_pending.contains(target))
					continue;
				if (paths.contains(target))
					continue;
				MUST(next_pending.insert(target));
				MUST(visited.insert(target));

				MUST(paths.insert(target, paths[current]));
				MUST(paths[target].push_back(target));
			}
		}

		pending = BAN::move(next_pending);
	}

	ASSERT_NOT_REACHED();
}

i64 puzzle1(FILE* fp)
{
	auto components = parse_components(fp);

	BAN::HashMap<BAN::String, i64> connection_count;

	srand(time(nullptr));
	for (size_t i = 0; i < 100; i++)
	{
		size_t idx1 = rand() % components.size();
		size_t idx2 = rand() % components.size();

		auto path = find_shortest_path(next(components.begin(), idx1)->key, next(components.begin(), idx2)->key, components);
		for (size_t j = 1; j < path.size(); j++)
		{
			const auto& connection = connection_key(path[j - 1], path[j]);
			if (!connection_count.contains(connection))
				MUST(connection_count.insert(connection, 0));
			connection_count[connection]++;
		}
	}

	struct Connection
	{
		BAN::String connection;
		i64 count;
	};
	BAN::Vector<Connection> hot_connections;
	for (const auto& [connection, count] : connection_count)
		MUST(hot_connections.emplace_back(connection, count));
	BAN::sort::sort(hot_connections.begin(), hot_connections.end(), [](const auto& a, const auto& b) { return a.count > b.count; });

	for (size_t depth = 0; depth < hot_connections.size(); depth++)
	{
		for (size_t i = 0; i < depth; i++)
		{
			for (size_t j = i + 1; j < depth; j++)
			{
				for (size_t k = j + 1; k < depth; k++)
				{
					BAN::HashSet<BAN::String> removed;
					MUST(removed.insert(hot_connections[i].connection));
					MUST(removed.insert(hot_connections[j].connection));
					MUST(removed.insert(hot_connections[k].connection));

					size_t lhs_size = graph_size(components, hot_connections[i].connection.sv().substring(0, 3), removed);
					if (lhs_size == components.size())
						continue;

					size_t rhs_size = graph_size(components, hot_connections[i].connection.sv().substring(3, 3), removed);
					if (rhs_size == components.size())
						continue;

					if (lhs_size + rhs_size == components.size())
						return lhs_size * rhs_size;
				}
			}
		}
	}

	return -1;
}

i64 puzzle2(FILE* fp)
{
	(void)fp;
	return -1;
}

int main(int argc, char** argv)
{
	const char* file_path = "/usr/share/aoc2023/day25_input.txt";

	if (argc >= 2)
		file_path = argv[1];

	FILE* fp = fopen(file_path, "r");
	if (fp == nullptr)
	{
		perror("fopen");
		return 1;
	}

	printf("puzzle1: %" PRId64 "\n", puzzle1(fp));

	fseek(fp, 0, SEEK_SET);

	printf("puzzle2: %" PRId64 "\n", puzzle2(fp));

	fclose(fp);
}
