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
			} pin_complex;
		};

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
