#include <LibDEFLATE/Compressor.h>
#include <LibDEFLATE/Utils.h>

#include <BAN/Array.h>
#include <BAN/Heap.h>
#include <BAN/Optional.h>
#include <BAN/Sort.h>

namespace LibDEFLATE
{

	constexpr size_t s_max_length = 258;
	constexpr size_t s_max_distance = 32768;

	constexpr size_t s_max_symbols = 288;
	constexpr uint8_t s_max_bits = 15;

	struct Leaf
	{
		uint16_t code;
		uint8_t length;
	};

	static BAN::ErrorOr<void> create_huffman_tree(BAN::Span<const size_t> freq, BAN::Span<Leaf> output)
	{
		ASSERT(freq.size() <= s_max_symbols);
		ASSERT(freq.size() == output.size());

		struct node_t
		{
			size_t symbol;
			size_t freq;
			node_t* left;
			node_t* right;
		};

#if LIBDEFLATE_AVOID_STACK
		BAN::Vector<node_t*> nodes;
		TRY(nodes.resize(s_max_symbols));
#else
		BAN::Array<node_t*, s_max_symbols> nodes;
#endif

		size_t node_count = 0;
		for (size_t sym = 0; sym < freq.size(); sym++)
		{
			if (freq[sym] == 0)
				continue;
			nodes[node_count] = static_cast<node_t*>(BAN::allocator(sizeof(node_t)));
			if (nodes[node_count] == nullptr)
			{
				for (size_t j = 0; j < node_count; j++)
					BAN::deallocator(nodes[j]);
				return BAN::Error::from_errno(ENOMEM);
			}
			*nodes[node_count++] = {
				.symbol = sym,
				.freq = freq[sym],
				.left = nullptr,
				.right = nullptr,
			};
		}

		for (auto& symbol : output)
			symbol = { .code = 0, .length = 0 };

		if (node_count == 0)
		{
			output[0] = { .code = 0, .length = 1 };
			return {};
		}

		static void (*free_tree)(node_t*) =
			[](node_t* root) -> void {
				if (root == nullptr)
					return;
				free_tree(root->left);
				free_tree(root->right);
				BAN::deallocator(root);
			};

		const auto comp =
			[](const node_t* a, const node_t* b) -> bool {
				if (a->freq != b->freq)
					return a->freq > b->freq;
				return a->symbol > b->symbol;
			};

		auto end_it = nodes.begin() + node_count;
		BAN::make_heap(nodes.begin(), end_it, comp);

		while (nodes.begin() + 1 != end_it)
		{
			node_t* parent = static_cast<node_t*>(BAN::allocator(sizeof(node_t)));
			if (parent == nullptr)
			{
				for (auto it = nodes.begin(); it != end_it; it++)
					free_tree(*it);
				return BAN::Error::from_errno(ENOMEM);
			}

			node_t* node1 = nodes.front();
			BAN::pop_heap(nodes.begin(), end_it--, comp);

			node_t* node2 = nodes.front();
			BAN::pop_heap(nodes.begin(), end_it--, comp);

			*parent = {
				.symbol = 0,
				.freq  = node1->freq + node2->freq,
				.left  = node1,
				.right = node2,
			};

			*end_it++ = parent;
			BAN::push_heap(nodes.begin(), end_it, comp);
		}

		static uint16_t (*gather_lengths)(const node_t*, BAN::Span<Leaf>, uint16_t) =
			[](const node_t* node, BAN::Span<Leaf> symbols, uint16_t depth) -> uint16_t {
				if (node == nullptr)
					return 0;
				uint16_t count = (depth > s_max_bits);
				if (node->left == nullptr && node->right == nullptr)
					symbols[node->symbol].length = BAN::Math::min<uint16_t>(depth, s_max_bits);
				else
				{
					count += gather_lengths(node->left,  symbols, depth + 1);
					count += gather_lengths(node->right, symbols, depth + 1);
				}
				return count;
			};

		const auto too_long_count = gather_lengths(nodes[0], output, 0);
		free_tree(nodes[0]);

		uint16_t bl_count[s_max_bits + 1] {};
		for (size_t sym = 0; sym < freq.size(); sym++)
			if (const uint8_t len = output[sym].length)
				bl_count[len]++;

		if (too_long_count > 0)
		{
			for (size_t i = 0; i < too_long_count / 2; i++)
			{
				uint16_t bits = s_max_bits - 1;
				while (bl_count[bits] == 0)
					bits--;
				bl_count[bits + 0]--;
				bl_count[bits + 1] += 2;
				bl_count[s_max_bits]--;
			}

			struct SymFreq
			{
				size_t symbol;
				size_t freq;
			};

			BAN::Vector<SymFreq> sym_freq;
			for (size_t sym = 0; sym < output.size(); sym++)
				if (freq[sym] != 0)
					TRY(sym_freq.push_back({ .symbol = sym, .freq = freq[sym] }));

			BAN::sort::sort(sym_freq.begin(), sym_freq.end(),
				[](auto a, auto b) { return a.freq < b.freq; }
			);

			size_t index = 0;
			for (uint16_t bits = s_max_bits; bits > 0; bits--)
				for (size_t i = 0; i < bl_count[bits]; i++)
					output[sym_freq[index++].symbol].length = bits;
			ASSERT(index == sym_freq.size());
		}

		uint16_t next_code[s_max_bits + 1] {};
		uint16_t code = 0;
		for (uint8_t bits = 1; bits <= s_max_bits; bits++)
		{
			code = (code + bl_count[bits - 1]) << 1;
			next_code[bits] = code;
		}

		for (size_t sym = 0; sym < freq.size(); sym++)
			if (const uint16_t len = output[sym].length)
				output[sym].code = next_code[len]++;

		return {};
	}

	struct Encoding
	{
		uint16_t symbol;
		uint16_t extra_data { 0 };
		uint8_t extra_len  { 0 };
	};

	static constexpr Encoding get_len_encoding(uint16_t length)
	{
		ASSERT(3 <= length && length <= s_max_length);

		constexpr uint16_t base[] {
			3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 15, 17, 19, 23, 27, 31, 35, 43, 51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 258
		};
		constexpr uint8_t extra_bits[] {
			0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0
		};
		constexpr size_t count = sizeof(base) / sizeof(*base);

		for (size_t i = 0;; i++)
		{
			if (i + 1 < count && length >= base[i + 1])
				continue;
			return {
				.symbol = static_cast<uint16_t>(257 + i),
				.extra_data = static_cast<uint16_t>(length - base[i]),
				.extra_len = extra_bits[i],
			};
		}
	}

	static constexpr Encoding get_dist_encoding(uint16_t distance)
	{
		ASSERT(1 <= distance && distance <= s_max_distance);

		constexpr uint16_t base[] {
			1, 2, 3, 4, 5, 7, 9, 13, 17, 25, 33, 49, 65, 97, 129, 193, 257, 385, 513, 769, 1025, 1537, 2049, 3073, 4097, 6145, 8193, 12289, 16385, 24577
		};
		constexpr uint8_t extra_bits[] {
			0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13
		};
		constexpr size_t count = sizeof(base) / sizeof(*base);

		for (size_t i = 0;; i++)
		{
			if (i + 1 < count && distance >= base[i + 1])
				continue;
			return {
				.symbol = static_cast<uint16_t>(i),
				.extra_data = static_cast<uint16_t>(distance - base[i]),
				.extra_len = extra_bits[i],
			};
		}
	}

	static void get_frequencies(BAN::Span<const Compressor::LZ77Entry> entries, BAN::Span<size_t> lit_len_freq, BAN::Span<size_t> dist_freq)
	{
		ASSERT(lit_len_freq.size() == 286);
		ASSERT(dist_freq.size() == 30);

		for (auto entry : entries)
		{
			switch (entry.type)
			{
				case Compressor::LZ77Entry::Type::Literal:
					lit_len_freq[entry.as.literal]++;
					break;
				case Compressor::LZ77Entry::Type::DistLength:
					lit_len_freq[get_len_encoding(entry.as.dist_length.length).symbol]++;
					dist_freq[get_dist_encoding(entry.as.dist_length.distance).symbol]++;
					break;
			}
		}

		lit_len_freq[256]++;
	}

	struct CodeLengthInfo
	{
		uint16_t hlit;
		uint8_t hdist;
		uint8_t hclen;
		BAN::Vector<Encoding> encoding;
		BAN::Array<uint8_t, 19> code_length;
		BAN::Array<Leaf, 19> code_length_tree;
	};

	static BAN::ErrorOr<CodeLengthInfo> build_code_length_info(BAN::Span<const Leaf> lit_len_tree, BAN::Span<const Leaf> dist_tree)
	{
		CodeLengthInfo result;

		const auto append_tree =
			[&result](BAN::Span<const Leaf>& tree) -> BAN::ErrorOr<void>
			{
				while (!tree.empty() && tree[tree.size() - 1].length == 0)
					tree = tree.slice(0, tree.size() - 1);

				for (size_t i = 0; i < tree.size();)
				{
					size_t count = 1;
					while (i + count < tree.size() && tree[i].length == tree[i + count].length)
						count++;

					if (tree[i].length == 0)
					{
						if (count > 138)
							count = 138;

						if (count < 3)
						{
							for (size_t j = 0; j < count; j++)
								TRY(result.encoding.push_back({ .symbol = 0 }));
						}
						else if (count < 11)
						{
							TRY(result.encoding.push_back({
								.symbol = 17,
								.extra_data = static_cast<uint8_t>(count - 3),
								.extra_len = 3,
							}));
						}
						else
						{
							TRY(result.encoding.push_back({
								.symbol = 18,
								.extra_data = static_cast<uint8_t>(count - 11),
								.extra_len = 7,
							}));
						}
					}
					else
					{
						if (count >= 3 && !result.encoding.empty() && result.encoding.back().symbol == tree[i].length)
						{
							if (count > 6)
								count = 6;
							TRY(result.encoding.push_back({
								.symbol = 16,
								.extra_data = static_cast<uint8_t>(count - 3),
								.extra_len = 2,
							}));
						}
						else
						{
							count = 1;
							TRY(result.encoding.push_back({ .symbol = tree[i].length }));
						}
					}

					i += count;
				}

				return {};
			};

		TRY(append_tree(lit_len_tree));
		result.hlit = lit_len_tree.size();

		TRY(append_tree(dist_tree));
		result.hdist = dist_tree.size();

		BAN::Array<size_t, 19> code_len_freq(0);
		for (auto entry : result.encoding)
			code_len_freq[entry.symbol]++;
		TRY(create_huffman_tree(code_len_freq.span(), result.code_length_tree.span()));

		constexpr uint8_t code_length_order[] {
			16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15
		};
		for (size_t i = 0; i < result.code_length_tree.size(); i++)
			result.code_length[i] = result.code_length_tree[code_length_order[i]].length;
		result.hclen = 19;
		while (result.hclen > 4 && result.code_length[result.hclen - 1] == 0)
			result.hclen--;

		return BAN::move(result);
	}

	uint32_t Compressor::get_hash_key(BAN::ConstByteSpan needle) const
	{
		ASSERT(needle.size() >= 3);
		return (needle[2] << 16) | (needle[1] << 8) | needle[0];
	}

	BAN::ErrorOr<void> Compressor::update_hash_chain(size_t count)
	{
		if (m_hash_chain.size() >= s_max_distance * 2)
		{
			const uint8_t* current = m_data.data() + m_hash_chain_index + count;
			for (auto& [_, chain] : m_hash_chain)
			{
				for (auto it = chain.begin(); it != chain.end(); it++)
				{
					const size_t distance = current - it->data();
					if (distance < s_max_distance)
						continue;

					while (it != chain.end())
						it = chain.remove(it);
					break;
				}
			}
		}

		for (size_t i = 0; i < count; i++)
		{
			auto slice = m_data.slice(m_hash_chain_index + i);
			if (slice.size() < 3)
				break;

			const uint32_t key = get_hash_key(slice);

			auto it = m_hash_chain.find(key);
			if (it != m_hash_chain.end())
				TRY(it->value.insert(it->value.begin(), slice));
			else
			{
				HashChain new_chain;
				TRY(new_chain.push_back(slice));
				TRY(m_hash_chain.insert(key, BAN::move(new_chain)));
			}
		}

		m_hash_chain_index += count;

		return {};
	}

	BAN::ErrorOr<Compressor::LZ77Entry> Compressor::find_longest_match(BAN::ConstByteSpan needle) const
	{
		LZ77Entry result = {
			.type = LZ77Entry::Type::Literal,
			.as = { .literal = needle[0] }
		};

		if (needle.size() < 3)
			return result;

		const uint32_t key = get_hash_key(needle);

		auto it = m_hash_chain.find(key);
		if (it == m_hash_chain.end())
			return result;

		auto& chain = it->value;
		for (const auto node : chain)
		{
			const size_t distance = needle.data() - node.data();
			if (distance > s_max_distance)
				break;

			size_t length = 3;
			const size_t max_length = BAN::Math::min(needle.size(), s_max_length);
			while (length < max_length && needle[length] == node[length])
				length++;

			if (result.type != LZ77Entry::Type::DistLength || length > result.as.dist_length.length)
			{
				result = LZ77Entry {
					.type = LZ77Entry::Type::DistLength,
					.as = {
						.dist_length = {
							.length = static_cast<uint16_t>(length),
							.distance = static_cast<uint16_t>(distance),
						}
					}
				};
			}
		}

		return result;
	}

	BAN::ErrorOr<BAN::Vector<Compressor::LZ77Entry>> Compressor::lz77_compress(BAN::ConstByteSpan data)
	{
		BAN::Vector<LZ77Entry> result;

		size_t advance = 0;
		for (size_t i = 0; i < data.size(); i += advance)
		{
			TRY(update_hash_chain(advance));

			auto match = TRY(find_longest_match(data.slice(i)));
			if (match.type == LZ77Entry::Type::Literal)
			{
				TRY(result.push_back(match));
				advance = 1;
				continue;
			}

			ASSERT(match.type == LZ77Entry::Type::DistLength);

			auto lazy_match = TRY(find_longest_match(data.slice(i + 1)));
			if (lazy_match.type == LZ77Entry::Type::DistLength && lazy_match.as.dist_length.length > match.as.dist_length.length)
			{
				TRY(result.push_back({ .type = LZ77Entry::Type::Literal, .as = { .literal = data[i] }}));
				TRY(result.push_back(lazy_match));
				advance = 1 + lazy_match.as.dist_length.length;
			}
			else
			{
				TRY(result.push_back(match));
				advance = match.as.dist_length.length;
			}
		}

		return result;
	}

	BAN::ErrorOr<void> Compressor::compress_block(BAN::ConstByteSpan data, bool final)
	{
		// FIXME: use fixed trees or uncompressed blocks

		auto lz77_entries = TRY(lz77_compress(data));

#if LIBDEFLATE_AVOID_STACK
		BAN::Vector<size_t> lit_len_freq, dist_freq;
		TRY(lit_len_freq.resize(286, 0));
		TRY(dist_freq.resize(30, 0));
#else
		BAN::Array<size_t, 286> lit_len_freq(0);
		BAN::Array<size_t, 30> dist_freq(0);
#endif

		get_frequencies(lz77_entries.span(), lit_len_freq.span(), dist_freq.span());

#if LIBDEFLATE_AVOID_STACK
		BAN::Vector<Leaf> lit_len_tree, dist_tree;
		TRY(lit_len_tree.resize(286));
		TRY(dist_tree.resize(30));
#else
		BAN::Array<Leaf, 286> lit_len_tree;
		BAN::Array<Leaf, 30> dist_tree;
#endif

		TRY(create_huffman_tree(lit_len_freq.span(), lit_len_tree.span()));
		TRY(create_huffman_tree(dist_freq.span(), dist_tree.span()));

		auto info = TRY(build_code_length_info(lit_len_tree.span(), dist_tree.span()));

		TRY(m_stream.write_bits(final, 1));
		TRY(m_stream.write_bits(2, 2));

		TRY(m_stream.write_bits(info.hlit - 257, 5));
		TRY(m_stream.write_bits(info.hdist - 1,  5));
		TRY(m_stream.write_bits(info.hclen - 4,  4));

		for (size_t i = 0; i < info.hclen; i++)
			TRY(m_stream.write_bits(info.code_length[i], 3));

		for (const auto entry : info.encoding)
		{
			const auto symbol = info.code_length_tree[entry.symbol];
			TRY(m_stream.write_bits(reverse_bits(symbol.code, symbol.length), symbol.length));
			TRY(m_stream.write_bits(entry.extra_data, entry.extra_len));
		}

		for (const auto entry : lz77_entries)
		{
			switch (entry.type)
			{
				case LZ77Entry::Type::Literal:
				{
					const auto symbol = lit_len_tree[entry.as.literal];
					TRY(m_stream.write_bits(reverse_bits(symbol.code, symbol.length), symbol.length));
					break;
				}
				case LZ77Entry::Type::DistLength:
				{
					const auto len_encoding = get_len_encoding(entry.as.dist_length.length);
					const auto len_code = lit_len_tree[len_encoding.symbol];
					TRY(m_stream.write_bits(reverse_bits(len_code.code, len_code.length), len_code.length));
					TRY(m_stream.write_bits(len_encoding.extra_data, len_encoding.extra_len));

					const auto dist_encoding = get_dist_encoding(entry.as.dist_length.distance);
					const auto dist_code = dist_tree[dist_encoding.symbol];
					TRY(m_stream.write_bits(reverse_bits(dist_code.code, dist_code.length), dist_code.length));
					TRY(m_stream.write_bits(dist_encoding.extra_data, dist_encoding.extra_len));

					break;
				}
			}
		}

		const auto end_code = lit_len_tree[256];
		TRY(m_stream.write_bits(reverse_bits(end_code.code, end_code.length), end_code.length));

		return {};
	}

	BAN::ErrorOr<BAN::Vector<uint8_t>> Compressor::compress()
	{
		uint32_t checksum = 0;
		switch (m_type)
		{
			case StreamType::Raw:
				break;
			case StreamType::Zlib:
				TRY(m_stream.write_bits(0x78, 8)); // deflate with 32k window
				TRY(m_stream.write_bits(0x9C, 8)); // default compression
				checksum = calculate_adler32(m_data);
				break;
		}

		constexpr size_t max_block_size = 16 * 1024;
		while (!m_data.empty())
		{
			const size_t block_size = BAN::Math::min<size_t>(m_data.size(), max_block_size);
			TRY(compress_block(m_data.slice(0, block_size), block_size == m_data.size()));
			m_data = m_data.slice(block_size);
		}

		TRY(m_stream.pad_to_byte_boundary());

		switch (m_type)
		{
			case StreamType::Raw:
				break;
			case StreamType::Zlib:
				TRY(m_stream.write_bits(checksum >> 24, 8));
				TRY(m_stream.write_bits(checksum >> 16, 8));
				TRY(m_stream.write_bits(checksum >>  8, 8));
				TRY(m_stream.write_bits(checksum >>  0, 8));
				break;
		}

		return m_stream.take_buffer();
	}

}
