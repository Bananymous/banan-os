#include <BAN/HashMap.h>
#include <BAN/HashSet.h>
#include <BAN/Sort.h>
#include <BAN/String.h>
#include <BAN/StringView.h>

#include <ctype.h>
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

static u32 name_to_u32(BAN::StringView name)
{
	ASSERT(name.size() == 3);
	return ((u32)name[0] << 16) | ((u32)name[1] << 8) | ((u32)name[2] << 0);
}

static BAN::String u32_to_name(u32 val)
{
	BAN::String result;
	MUST(result.push_back(val >> 16));
	MUST(result.push_back(val >>  8));
	MUST(result.push_back(val >>  0));
	return result;
}

struct Op
{
	enum { OR, AND, XOR } type { OR };
	u32 src1 { 0 };
	u32 src2 { 0 };
	u32 dst  { 0 };
};

struct ParseInputResult
{
	BAN::HashMap<u32, u8> wires;
	BAN::Vector<Op> ops;
};

static ParseInputResult parse_input(FILE* fp)
{
	char buffer[128];

	BAN::HashMap<u32, u8> wires;
	while (fgets(buffer, sizeof(buffer), fp))
	{
		if (buffer[0] == '\n')
			break;
		MUST(wires.insert(name_to_u32(BAN::StringView(buffer).substring(0, 3)), buffer[5] - '0'));
	}

	BAN::Vector<Op> ops;
	while (fgets(buffer, sizeof(buffer), fp))
	{
		auto parts = MUST(BAN::StringView(buffer).split([](char c) -> bool { return isspace(c); }));
		MUST(ops.emplace_back(
			(parts[1] == "OR"_sv) ? Op::OR : (parts[1] == "AND"_sv) ? Op::AND : Op::XOR,
			name_to_u32(parts[0]),
			name_to_u32(parts[2]),
			name_to_u32(parts[4])
		));
	}

	return {
		.wires = BAN::move(wires),
		.ops = BAN::move(ops),
	};
}

i64 part1(FILE* fp)
{
	auto [wires, ops] = parse_input(fp);

	BAN::HashSet<u32> defined_wires;
	for (const auto [wire, _] : wires)
		MUST(defined_wires.insert(wire));

	usize ops_done_count = 0;
	BAN::Vector<u8> ops_done_map;
	MUST(ops_done_map.resize((ops.size() + 7) / 8, 0));

	while (ops_done_count < ops.size())
	{
		for (usize i = 0; i < ops.size(); i++)
		{
			if (ops_done_map[i / 8] & (1 << (i % 8)))
				continue;

			if (!defined_wires.contains(ops[i].src1) || !defined_wires.contains(ops[i].src2))
				continue;

			const u8 src1 = wires[ops[i].src1];
			const u8 src2 = wires[ops[i].src2];

			u8 val = 0;
			switch (ops[i].type)
			{
				case Op::OR:  val = src1 | src2; break;
				case Op::AND: val = src1 & src2; break;
				case Op::XOR: val = src1 ^ src2; break;
			}
			MUST(wires.insert_or_assign(ops[i].dst, val));

			MUST(defined_wires.insert(ops[i].dst));
			ops_done_count++;
			ops_done_map[i / 8] |= (1 << (i % 8));
		}
	}

	u64 result = 0;
	for (usize bit = 0; bit < 64; bit++)
	{
		char name[4];
		name[0] = 'z';
		name[1] = (bit / 10) + '0';
		name[2] = (bit % 10) + '0';
		name[3] = '\0';

		auto it = wires.find(name_to_u32(name));
		if (it == wires.end())
			break;
		result |= (u64)it->value << bit;
	}

	return result;
}

static bool has_sources(Op op, u32 src1, u32 src2)
{
	if (op.src1 == src1 && op.src2 == src2)
		return true;
	if (op.src1 == src2 && op.src2 == src1)
		return true;
	return false;
}

struct u32Triplet
{
	u32 op_and, op_xor, op_or;
};

static u32Triplet find_ops(const BAN::Vector<Op>& ops, u32 src1, u32 src2)
{
	u32 op_and = 0, op_xor = 0, op_or = 0;
	for (auto op : ops)
	{
		if (!has_sources(op, src1, src2))
			continue;
		switch (op.type)
		{
			case Op::AND: op_and = op.dst; break;
			case Op::XOR: op_xor = op.dst; break;
			case Op::OR:  op_or  = op.dst; break;
		}
	}
	return { op_and, op_xor, op_or };
}

BAN::String part2(FILE* fp)
{
	auto [wires, ops] = parse_input(fp);

	BAN::Vector<u32> swapped;

	u32 carry = 0;
	for (usize bit = 0; bit < 64; bit++)
	{
		dprintln("bit {}", bit);

		char name[4];
		name[1] = (bit / 10) + '0';
		name[2] = (bit % 10) + '0';
		name[3] = '\0';

		name[0] = 'x';
		const u32 src1 = name_to_u32(name);

		name[0] = 'y';
		const u32 src2 = name_to_u32(name);

		name[0] = 'z';
		const u32 dst = name_to_u32(name);

		auto [src_and, src_xor, _1] = find_ops(ops, src1, src2);
		if (!src_and)
			break;
		ASSERT(src_xor);

		if (bit == 0)
		{
			carry = src_and;
			continue;
		}

		auto [dst_and, dst_xor, _2] = find_ops(ops, carry, src_xor);
		if (dst_xor == 0 && dst_and == 0)
		{
			dwarnln("swapped src xor, src and");
			MUST(swapped.push_back(src_xor));
			MUST(swapped.push_back(src_and));
			BAN::swap(src_xor, src_and);

			auto [tmp_and, tmp_xor, _] = find_ops(ops, carry, src_xor);
			dst_and = tmp_and;
			dst_xor = tmp_xor;
		}
		else if (dst_xor != dst)
		{
			if (dst_and == dst)
			{
				dwarnln("swapped dst xor, dst and");
				MUST(swapped.push_back(dst_xor));
				MUST(swapped.push_back(dst_and));
				BAN::swap(dst_xor, dst_and);
			}
			else if (src_and == dst)
			{
				dwarnln("swapped src and, dst xor");
				MUST(swapped.push_back(src_and));
				MUST(swapped.push_back(dst_xor));
				BAN::swap(src_and, dst_xor);
			}
			else if (src_xor == dst)
			{
				dwarnln("swapped src xor, dst xor");
				MUST(swapped.push_back(src_xor));
				MUST(swapped.push_back(dst_xor));
				BAN::swap(src_and, dst_xor);
			}
			else
			{
				auto [_1, _2, tmp_or] = find_ops(ops, src_and, dst_and);
				if (tmp_or == dst)
				{
					dwarnln("swapped carry, dst xor");
					MUST(swapped.push_back(dst_xor));
					MUST(swapped.push_back(tmp_or));
					carry = dst_xor;
					continue;
				}
				else
				{
					dwarnln("invalid ({}, {}, {})", u32_to_name(dst_xor), u32_to_name(dst_and), u32_to_name(dst));
					ASSERT_NOT_REACHED();
				}
			}
		}

		auto [_3, _4, tmp_or] = find_ops(ops, src_and, dst_and);
		carry = tmp_or;
	}

	ASSERT(swapped.size() == 8);
	BAN::sort::sort(swapped.begin(), swapped.end());

	BAN::String result;
	for (const auto& swap : swapped)
		MUST(result.append(MUST(BAN::String::formatted("{},", u32_to_name(swap)))));
	result.pop_back();
	return result;
}

int main(int argc, char** argv)
{
	const char* file_path = "/usr/share/aoc2024/day24_input.txt";

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
