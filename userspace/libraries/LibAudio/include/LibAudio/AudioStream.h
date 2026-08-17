#pragma once

#include <BAN/StringView.h>
#include <BAN/UniqPtr.h>

namespace LibAudio
{

	class AudioStream
	{
		BAN_NON_COPYABLE(AudioStream);
		BAN_NON_MOVABLE(AudioStream);

	public:
		static BAN::ErrorOr<BAN::UniqPtr<AudioStream>> load(BAN::StringView path);
		virtual ~AudioStream();

		virtual uint32_t channels() const = 0;
		virtual uint32_t sample_rate() const = 0;
		virtual uint32_t samples_remaining() const = 0;

		virtual float get_sample() = 0;

	protected:
		AudioStream() = default;

	private:
		void* m_mmap_addr { nullptr };
		size_t m_mmap_size { 0 };
	};

}
