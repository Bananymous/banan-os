#pragma once


#include <BAN/ByteSpan.h>
#include <BAN/NoCopyMove.h>
#include <BAN/Vector.h>

#include <LibDEFLATE/BitStream.h>
#include <LibDEFLATE/HuffmanTree.h>
#include <LibDEFLATE/StreamType.h>

namespace LibDEFLATE
{

	class Decompressor
	{
		BAN_NON_COPYABLE(Decompressor);
		BAN_NON_MOVABLE(Decompressor);

	public:
		Decompressor(BAN::ConstByteSpan data, StreamType type)
			: m_type(type)
			, m_stream(data)
		{ }

		BAN::ErrorOr<BAN::Vector<uint8_t>> decompress();

	private:
		BAN::ErrorOr<uint16_t> read_symbol(const HuffmanTree& tree);
		BAN::ErrorOr<void> inflate_block(const HuffmanTree& length_tree, const HuffmanTree& distance_tree);

		BAN::ErrorOr<void> decompress_type0();
		BAN::ErrorOr<void> decompress_type1();
		BAN::ErrorOr<void> decompress_type2();

		BAN::ErrorOr<void> handle_header();
		BAN::ErrorOr<void> handle_footer();

	private:
		const StreamType m_type;
		BitInputStream m_stream;
		BAN::Vector<uint8_t> m_output;
		BAN::Optional<HuffmanTree> m_fixed_tree;
	};

}
