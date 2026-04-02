#pragma once

#include <BAN/Vector.h>

namespace Kernel::HDAudio
{

	struct CORBEntry
	{
		union {
			struct {
				uint32_t data          : 8;
				uint32_t command       : 12;
				uint32_t node_index    : 8;
				uint32_t codec_address : 4;
			};
			uint32_t raw;
		};
	};
	static_assert(sizeof(CORBEntry) == sizeof(uint32_t));

	struct BDLEntry
	{
		paddr_t address;
		uint32_t length;
		uint32_t ioc;
	};
	static_assert(sizeof(BDLEntry) == 16);

	struct AFGWidget
	{
		enum class Type
		{
			OutputConverter,
			InputConverter,
			Mixer,
			Selector,
			PinComplex,
			Power,
			VolumeKnob,
			BeepGenerator,
		};

		Type type;
		uint8_t id;

		union
		{
			struct
			{
				bool input;
				bool output;
				bool display; // HDMI or DP
				uint32_t config;
			} pin_complex;
		};

		struct Amplifier
		{
			uint8_t offset;
			uint8_t num_steps;
			uint8_t step_size;
			bool mute;
		};

		BAN::Optional<Amplifier> output_amplifier;

		BAN::Vector<uint16_t> connections;
	};

	struct AFGNode
	{
		uint8_t id;
		BAN::Vector<AFGWidget> widgets;
	};

	struct Codec
	{
		uint8_t id;
		BAN::Vector<AFGNode> nodes;
	};

	enum class StreamType
	{
		Input,
		Output,
		Bidirectional,
	};

}
