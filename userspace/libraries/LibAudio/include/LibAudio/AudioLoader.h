#pragma once

#include <BAN/StringView.h>
#include <BAN/UniqPtr.h>

namespace LibAudio
{

	class AudioLoader
	{
		BAN_NON_COPYABLE(AudioLoader);
		BAN_NON_MOVABLE(AudioLoader);

	public:
		static BAN::ErrorOr<BAN::UniqPtr<AudioLoader>> load(BAN::StringView path);
		virtual ~AudioLoader();

		virtual uint32_t channels() const = 0;
		virtual uint32_t sample_rate() const = 0;
		virtual uint32_t samples_remaining() const = 0;

		virtual double get_sample() = 0;

	protected:
		AudioLoader() = default;

	private:
		void* m_mmap_addr { nullptr };
		size_t m_mmap_size { 0 };
	};

}
