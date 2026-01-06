#pragma once

#include <BAN/StringView.h>

namespace LibAudio
{

	struct Packet
	{
		enum : uint8_t
		{
			Notify,         // parameter: ignored
			                // response: nothing
			                // server refereshes buffered data

			RegisterBuffer, // paramenter: smo key
			                // response: nothing
			                // register audio buffer to server

			QueryDevices,   // parameter: ignored
			                // response: (uint32_t)
							// query the number of devices available

			QueryPins,      // parameter: sink number
			                // response: (uint32_t)
							// query the number of pins the sink has

			GetDevice,      // parameter: ignored
			                // reponse: (uint32_t)
							// get the currently active device

			SetDevice,      // parameter: device number
			                // reponse: nothing
							// set the currently active device

			GetPin,         // parameter: ignored
			                // response: nothing
							// get the active pin of the current device

			SetPin,         // parameter: pin number
			                // response: nothing
							// set the active pin of the current device
		} type;

		uint64_t parameter;
	};

	static constexpr BAN::StringView s_audio_server_socket = "/tmp/audio-server.socket"_sv;

}
