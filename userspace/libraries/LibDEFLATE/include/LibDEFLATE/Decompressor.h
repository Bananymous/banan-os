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
		enum class Status
		{
			Done,
			NeedMoreInput,
			NeedMoreOutput,
		};

	public:
		Decompressor(StreamType type)
			: m_type(type)
		{ }

		BAN::ErrorOr<BAN::Vector<uint8_t>> decompress(BAN::ConstByteSpan input);
		BAN::ErrorOr<BAN::Vector<uint8_t>> decompress(BAN::Span<const BAN::ConstByteSpan> input);

		BAN::ErrorOr<Status> decompress(BAN::ConstByteSpan input, size_t& input_consumed, BAN::ByteSpan output, size_t& output_produced);

	private:
		BAN::ErrorOr<uint16_t> read_symbol(const HuffmanTree& tree);

		BAN::ErrorOr<void> handle_header();
		BAN::ErrorOr<void> handle_footer();
		BAN::ErrorOr<void> handle_dynamic_header();
		BAN::ErrorOr<void> handle_symbol();

		void write_data_to_output(BAN::ByteSpan&);

	private:
		enum class State
		{
			StreamHeader,
			StreamFooter,
			BlockHeader,
			LiteralHeader,
			DynamicHeader,
			ReadRaw,
			Symbol,
			Done,
		};

	private:
		const StreamType m_type;

		State m_state { State::StreamHeader };

		BitInputStream m_stream;

		static constexpr size_t total_window_size = 32 * 1024;
		BAN::Vector<uint8_t> m_window;
		size_t m_window_size { 0 };
		size_t m_window_tail { 0 };
		size_t m_produced_bytes { 0 };

		bool m_bfinal { false };
		HuffmanTree m_length_tree;
		HuffmanTree m_distance_tree;

		uint16_t m_raw_bytes_left { 0 };

		union
		{
			struct {
				uint32_t s1;
				uint32_t s2;
				uint32_t adler32;
			} zlib;
			struct {
				uint32_t crc32;
				uint32_t isize;
			} gzip;
		} m_stream_info;
	};

}
