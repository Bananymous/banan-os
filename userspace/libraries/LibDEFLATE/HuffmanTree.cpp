#include <LibDEFLATE/HuffmanTree.h>

namespace LibDEFLATE
{

	HuffmanTree& HuffmanTree::operator=(HuffmanTree&& other)
	{
		m_instant_bits = other.m_instant_bits;
		m_min_bits = other.m_min_bits;
		m_max_bits = other.m_max_bits;

		m_instant = BAN::move(other.m_instant);
		m_min_code = BAN::move(other.m_min_code);
		m_slow_table = BAN::move(other.m_slow_table);

		return *this;
	}

	BAN::ErrorOr<HuffmanTree> HuffmanTree::create(BAN::Span<const uint8_t> bit_lengths)
	{
		HuffmanTree result;
		TRY(result.initialize(bit_lengths));
		return result;
	}

	BAN::ErrorOr<void> HuffmanTree::initialize(BAN::Span<const uint8_t> bit_lengths)
	{
		m_max_bits = 0;
		m_min_bits = MAX_BITS;

		uint16_t max_sym = 0;
		uint16_t bl_count[MAX_BITS + 1] {};
		for (size_t sym = 0; sym < bit_lengths.size(); sym++)
		{
			if (bit_lengths[sym] == 0)
				continue;
			m_max_bits = BAN::Math::max(bit_lengths[sym], m_max_bits);
			m_min_bits = BAN::Math::min(bit_lengths[sym], m_min_bits);
			bl_count[bit_lengths[sym]]++;
			max_sym = sym;
		}

		uint16_t next_code[MAX_BITS + 1] {};

		uint16_t code = 0;
		for (uint8_t bits = 1; bits <= MAX_BITS; bits++)
		{
			code = (code + bl_count[bits - 1]) << 1;
			next_code[bits] = code;
			m_min_code[bits] = code;
		}

		BAN::Vector<Leaf> tree;
		TRY(tree.resize(max_sym + 1, { .code = 0, .len = 0 }));
		for (uint16_t sym = 0; sym <= max_sym; sym++)
		{
			tree[sym].len = bit_lengths[sym];
			if (const uint8_t len = tree[sym].len)
				tree[sym].code = next_code[len]++;
		}

		TRY(build_instant_table(tree.span()));
		TRY(build_slow_table(tree.span()));

		return {};
	}

	BAN::ErrorOr<void> HuffmanTree::build_instant_table(BAN::Span<const Leaf> tree)
	{
		m_instant_bits = BAN::Math::min<uint8_t>(9, m_max_bits);
		TRY(m_instant.resize(1 << m_instant_bits, {}));

		for (uint16_t sym = 0; sym < tree.size(); sym++)
		{
			if (tree[sym].len == 0 || tree[sym].len > m_instant_bits)
				continue;
			const uint16_t code = tree[sym].code;
			const uint16_t shift = m_instant_bits - tree[sym].len;
			for (uint16_t j = code << shift; j < (code + 1) << shift; j++)
				m_instant[j] = { sym, tree[sym].len };
		}

		return {};
	}

	BAN::ErrorOr<void> HuffmanTree::build_slow_table(BAN::Span<const Leaf> tree)
	{
		TRY(m_slow_table.resize(MAX_BITS + 1));
		for (uint16_t sym = 0; sym < tree.size(); sym++)
		{
			const auto leaf = tree[sym];
			if (leaf.len == 0)
				continue;
			const size_t offset = leaf.code - m_min_code[leaf.len];
			if (offset >= m_slow_table[leaf.len].size())
				TRY(m_slow_table[leaf.len].resize(offset + 1));
			m_slow_table[leaf.len][offset] = sym;
		}

		return {};
	}


	BAN::ErrorOr<HuffmanTree> HuffmanTree::fixed_tree()
	{
		struct BitLengths
		{
			consteval BitLengths()
			{
				size_t i = 0;
				for (; i <= 143; i++) values[i] = 8;
				for (; i <= 255; i++) values[i] = 9;
				for (; i <= 279; i++) values[i] = 7;
				for (; i <= 287; i++) values[i] = 8;
			}

			BAN::Array<uint8_t, 288> values;
		};
		static constexpr BitLengths bit_lengths;
		return TRY(HuffmanTree::create(bit_lengths.values.span()));
	}

	BAN::Optional<HuffmanTree::Instant> HuffmanTree::get_symbol_instant(uint16_t code) const
	{
		ASSERT(code < m_instant.size());
		if (const auto entry = m_instant[code]; entry.len)
			return entry;
		return {};
	}

	BAN::Optional<uint16_t> HuffmanTree::get_symbol(uint16_t code, uint8_t len) const
	{
		ASSERT(len <= m_max_bits);
		const auto& symbols = m_slow_table[len];
		const size_t offset = code - m_min_code[len];
		if (symbols.size() <= offset)
			return {};
		return symbols[offset];
	}

}
