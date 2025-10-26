#pragma once

#include <BAN/Array.h>
#include <BAN/NoCopyMove.h>
#include <BAN/Optional.h>
#include <BAN/Vector.h>

namespace LibDEFLATE
{

	class HuffmanTree
	{
		BAN_NON_COPYABLE(HuffmanTree);

	public:
		static constexpr uint8_t MAX_BITS = 15;

		struct Leaf
		{
			uint16_t code;
			uint8_t len;
		};

		struct Instant
		{
			uint16_t symbol;
			uint8_t len;
		};

		HuffmanTree() {}
		HuffmanTree(HuffmanTree&& other) { *this = BAN::move(other); }
		HuffmanTree& operator=(HuffmanTree&& other);

		static BAN::ErrorOr<HuffmanTree> create(BAN::Span<const uint8_t> bit_lengths);

		static BAN::ErrorOr<HuffmanTree> fixed_tree();
		BAN::Optional<Instant> get_symbol_instant(uint16_t code) const;

		BAN::Optional<uint16_t> get_symbol(uint16_t code, uint8_t len) const;

		uint8_t instant_bits() const { return m_instant_bits; }
		uint8_t min_bits() const { return m_min_bits; }
		uint8_t max_bits() const { return m_max_bits; }
		bool empty() const { return m_min_bits == 0; }

	private:
		BAN::ErrorOr<void> initialize(BAN::Span<const uint8_t> bit_lengths);
		BAN::ErrorOr<void> build_instant_table(BAN::Span<const Leaf> tree);
		BAN::ErrorOr<void> build_slow_table(BAN::Span<const Leaf> tree);

	private:
		uint8_t m_instant_bits { 0 };
		uint8_t m_min_bits { 0 };
		uint8_t m_max_bits { 0 };

		BAN::Vector<Instant> m_instant;
		BAN::Array<uint16_t, MAX_BITS + 1> m_min_code;
		BAN::Vector<BAN::Vector<uint16_t>> m_slow_table;
	};

}
