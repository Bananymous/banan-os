#pragma once

#include <BAN/ByteSpan.h>
#include <BAN/HashMap.h>
#include <BAN/LinkedList.h>
#include <BAN/NoCopyMove.h>
#include <BAN/Vector.h>

#include <LibDEFLATE/BitStream.h>
#include <LibDEFLATE/StreamType.h>

namespace LibDEFLATE
{

	class Compressor
	{
		BAN_NON_COPYABLE(Compressor);
		BAN_NON_MOVABLE(Compressor);

	public:
		using HashChain = BAN::LinkedList<BAN::ConstByteSpan>;

		struct LZ77Entry
		{
			enum class Type
			{
				Literal,
				DistLength,
			} type;
			union
			{
				uint8_t literal;
				struct
				{
					uint16_t length;
					uint16_t distance;
				} dist_length;
			} as;
		};

	public:
		Compressor(BAN::ConstByteSpan data, StreamType type)
			: m_type(type)
			, m_data(data)
		{ }

		BAN::ErrorOr<BAN::Vector<uint8_t>> compress();

	private:
		BAN::ErrorOr<void> compress_block(BAN::ConstByteSpan, bool final);

		uint32_t get_hash_key(BAN::ConstByteSpan needle) const;
		BAN::ErrorOr<void> update_hash_chain(size_t count);

		BAN::ErrorOr<LZ77Entry> find_longest_match(BAN::ConstByteSpan needle) const;
		BAN::ErrorOr<BAN::Vector<LZ77Entry>> lz77_compress(BAN::ConstByteSpan data);

	private:
		const StreamType m_type;
		BAN::ConstByteSpan m_data;
		BitOutputStream m_stream;

		size_t m_hash_chain_index { 0 };
		BAN::HashMap<uint32_t, HashChain> m_hash_chain;
	};

}
