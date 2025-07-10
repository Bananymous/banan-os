#pragma once

#include <LibAudio/AudioLoader.h>

#include <BAN/ByteSpan.h>

namespace LibAudio
{

	class WAVAudioLoader : public AudioLoader
	{
	public:
		enum FormatCode : uint16_t
		{
			WAVE_FORMAT_PCM        = 0x01,
			WAVE_FORMAT_IEEE_FLOAT = 0x03,
		};

	public:
		static bool can_load_from(BAN::ConstByteSpan data);
		static BAN::ErrorOr<BAN::UniqPtr<AudioLoader>> create(BAN::ConstByteSpan data);

		uint32_t channels() const override { return m_channels; }
		uint32_t sample_rate() const override { return m_sample_rate; }
		uint32_t samples_remaining() const override { return m_total_samples - m_current_sample; }

		double get_sample() override;

	private:
		uint32_t m_channels { 0 };
		uint32_t m_sample_rate { 0 };
		uint32_t m_total_samples { 0 };

		FormatCode m_sample_format;
		uint16_t m_bits_per_sample { 0 };
		size_t m_current_sample { 0 };
		BAN::ConstByteSpan m_sample_data;
	};

}
