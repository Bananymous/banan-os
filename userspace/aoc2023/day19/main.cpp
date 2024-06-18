#include <BAN/Array.h>
#include <BAN/HashMap.h>
#include <BAN/HashSet.h>
#include <BAN/Sort.h>
#include <BAN/String.h>
#include <BAN/Vector.h>

#include <sys/time.h>

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

enum Variable { x, m, a, s };
enum class Comparison { Less, Greater, Always };

struct Rule
{
	Variable	operand1;
	i64			operand2;
	Comparison	comparison;

	BAN::String	target;
};

using Workflows = BAN::HashMap<BAN::String, BAN::Vector<Rule>>;

struct Item
{
	i64 values[4];

	bool operator==(const Item& other) const
	{
		return memcmp(values, other.values, sizeof(values)) == 0;
	}
};

struct ItemHash
{
	BAN::hash_t operator()(const Item& item) const
	{
		return BAN::hash<u64>()(item.values[0]) ^
			   BAN::hash<u64>()(item.values[1]) ^
			   BAN::hash<u64>()(item.values[2]) ^
			   BAN::hash<u64>()(item.values[3]);
	};
};

i64 parse_i64(BAN::StringView str)
{
	i64 result = 0;
	for (size_t i = 0; i < str.size() && isdigit(str[i]); i++)
		result = (result * 10) + (str[i] - '0');
	return result;
}

bool get_line(FILE* fp, BAN::String& out)
{
	out.clear();

	bool success = false;

	char buffer[128];
	while (fgets(buffer, sizeof(buffer), fp))
	{
		success = true;

		MUST(out.append(buffer));
		if (out.back() == '\n')
		{
			out.pop_back();
			break;
		}
	}

	return success;
}

Workflows parse_workflows(FILE* fp)
{
	Workflows workflows;

	BAN::String line;
	while (get_line(fp, line) && !line.empty())
	{
		auto name = line.sv().substring(0, *line.sv().find('{'));
		MUST(workflows.emplace(name));

		auto rule_str = line.sv().substring(*line.sv().find('{') + 1);
		rule_str = rule_str.substring(0, rule_str.size() - 1);

		auto rules_str = MUST(rule_str.split(','));

		for (size_t i = 0; i < rules_str.size() - 1; i++)
		{
			Rule rule;
			rule.operand1	=	(rules_str[i][0] == 'x') ? Variable::x :
								(rules_str[i][0] == 'm') ? Variable::m :
								(rules_str[i][0] == 'a') ? Variable::a :
														   Variable::s;
			rule.operand2	= parse_i64(rules_str[i].substring(2));
			rule.comparison	= (rules_str[i][1] == '<') ? Comparison::Less : Comparison::Greater;
			rule.target		= rules_str[i].substring(*rules_str[i].find(':') + 1);

			MUST(workflows[name].push_back(BAN::move(rule)));
		}

		MUST(workflows[name].emplace_back(Variable::x, 0, Comparison::Always, rules_str.back()));
	}

	return workflows;
}

BAN::Vector<Item> parse_items(FILE* fp)
{
	BAN::Vector<Item> items;

	BAN::String line;
	while (get_line(fp, line) && !line.empty())
	{
		auto values = MUST(line.sv().substring(1, line.size() - 2).split(','));
		ASSERT(values.size() == 4);
		ASSERT(values[0][0] == 'x');
		ASSERT(values[1][0] == 'm');
		ASSERT(values[2][0] == 'a');
		ASSERT(values[3][0] == 's');

		Item item;
		item.values[0] = parse_i64(values[0].substring(2));
		item.values[1] = parse_i64(values[1].substring(2));
		item.values[2] = parse_i64(values[2].substring(2));
		item.values[3] = parse_i64(values[3].substring(2));

		MUST(items.push_back(item));
	}

	return items;
}

bool satifies_rule(const Item& item, const Rule& rule)
{
	switch (rule.comparison)
	{
		case Comparison::Always:
			return true;
		case Comparison::Less:
			return item.values[rule.operand1] < rule.operand2;
		case Comparison::Greater:
			return item.values[rule.operand1] > rule.operand2;
	}
	ASSERT_NOT_REACHED();
}

bool is_accepted(const Item& item, const BAN::String& name, const Workflows& workflows)
{
	const auto& workflow = workflows[name];
	for (const auto& rule : workflow)
	{
		if (!satifies_rule(item, rule))
			continue;
		if (rule.target == "A"_sv)
			return true;
		if (rule.target == "R"_sv)
			return false;
		return is_accepted(item, rule.target, workflows);
	}
	ASSERT_NOT_REACHED();
}

i64 puzzle1(FILE* fp)
{
	auto workflows = parse_workflows(fp);
	auto items = parse_items(fp);

	BAN::Vector<Item> accepted;

	for (const auto& item : items)
		if (is_accepted(item, "in"_sv, workflows))
			MUST(accepted.push_back(item));

	i64 result = 0;
	for (const auto& item : accepted)
		result += item.values[0] + item.values[1] + item.values[2] + item.values[3];
	return result;
}

i64 puzzle2(FILE* fp)
{
	auto workflows = parse_workflows(fp);

	BAN::Array<BAN::HashSet<i64>, 4> values;
	for (const auto& workflow : workflows)
	{
		for (const auto& rule : workflow.value)
		{
			if (rule.comparison != Comparison::Always)
			{
				MUST(values[rule.operand1].insert(rule.operand2));
				MUST(values[rule.operand1].insert(rule.operand2 + 1));
			}
		}
	}
	for (i64 i = 0; i < 4; i++)
	{
		MUST(values[i].insert(1));
		MUST(values[i].insert(4001));
	}

	BAN::Array<BAN::Vector<i64>, 4> values_sorted;
	for (i64 i = 0; i < 4; i++)
	{
		MUST(values_sorted[i].reserve(values[i].size()));
		for (i64 value : values[i])
			MUST(values_sorted[i].push_back(value));
		BAN::sort::sort(values_sorted[i].begin(), values_sorted[i].end());
	}

	i64 result = 0;
	for (u64 xi = 0; xi < values_sorted[0].size() - 1; xi++)
	{
		timespec time_start;
		clock_gettime(CLOCK_MONOTONIC, &time_start);

		for (u64 mi = 0; mi < values_sorted[1].size() - 1; mi++)
		{
			for (u64 ai = 0; ai < values_sorted[2].size() - 1; ai++)
			{
				for (u64 si = 0; si < values_sorted[3].size() - 1; si++)
				{
					Item item {{
						values_sorted[0][xi],
						values_sorted[1][mi],
						values_sorted[2][ai],
						values_sorted[3][si]
					}};
					if (!is_accepted(item, "in"_sv, workflows))
						continue;

					i64 x_count = values_sorted[0][xi + 1] - values_sorted[0][xi];
					i64 m_count = values_sorted[1][mi + 1] - values_sorted[1][mi];
					i64 a_count = values_sorted[2][ai + 1] - values_sorted[2][ai];
					i64 s_count = values_sorted[3][si + 1] - values_sorted[3][si];

					result += x_count * m_count * a_count * s_count;
				}
			}
		}

		timespec time_stop;
		clock_gettime(CLOCK_MONOTONIC, &time_stop);

		u64 duration_us = (time_stop.tv_sec * 1'000'000 + time_stop.tv_nsec / 1'000) - (time_start.tv_sec * 1'000'000 + time_start.tv_nsec / 1'000);
		printf("step took %" PRIu64 ".%03" PRIu64 " ms, estimate %" PRIu64 " s\n", duration_us / 1000, duration_us % 1000, (values_sorted[0].size() - xi - 2) * duration_us / 1'000'000);
	}

	return result;
}

int main(int argc, char** argv)
{
	const char* file_path = "/usr/share/aoc2023/day19_input.txt";

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
