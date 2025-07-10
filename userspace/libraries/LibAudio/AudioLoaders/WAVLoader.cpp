#include <LibAudio/AudioLoaders/WAVLoader.h>

namespace LibAudio
{

	struct WAVChunk
	{
		char chunk_id[4];
		uint32_t chunk_size;
	};

	struct RIFFChunk : WAVChunk
	{
		char wave_id[4];
	};

	struct FormatChunk : WAVChunk
	{
		uint16_t wFormatTag;
		uint16_t nChannels;
		uint32_t nSamplePerSec;
		uint32_t nAvgBytePerSec;
		uint16_t nBlockAlign;
		uint16_t wBitsPerSample;
	};

	bool WAVAudioLoader::can_load_from(BAN::ConstByteSpan data)
	{
		if (data.size() < sizeof(RIFFChunk))
			return false;

		const auto riff_chunk = data.as<const RIFFChunk>();
		if (memcmp(riff_chunk.chunk_id, "RIFF", 4) != 0)
			return false;
		if (memcmp(riff_chunk.wave_id, "WAVE", 4) != 0)
			return false;
		return true;
	}

	BAN::ErrorOr<BAN::UniqPtr<AudioLoader>> WAVAudioLoader::create(BAN::ConstByteSpan data)
	{
		ASSERT(can_load_from(data));

		{
			const auto riff_chunk = data.as<const RIFFChunk>();
			if (sizeof(WAVChunk) + riff_chunk.chunk_size > data.size())
				return BAN::Error::from_errno(ENOBUFS);
			data = data.slice(0, sizeof(WAVChunk) + riff_chunk.chunk_size);
			data = data.slice(sizeof(RIFFChunk));
		}

		BAN::Optional<FormatChunk> format_chunk;
		BAN::ConstByteSpan sample_data;

		while (!data.empty())
		{
			const auto chunk = data.as<const WAVChunk>();
			if (data.size() < sizeof(WAVChunk) + chunk.chunk_size)
				return BAN::Error::from_errno(ENOBUFS);

			if (memcmp(chunk.chunk_id, "fmt ", 4) == 0)
				format_chunk = data.as<const FormatChunk>();
			else if (memcmp(chunk.chunk_id, "data", 4) == 0)
				sample_data = data.slice(sizeof(WAVChunk), chunk.chunk_size);

			data = data.slice(sizeof(WAVChunk) + chunk.chunk_size);
		}

		if (!format_chunk.has_value() || sample_data.empty())
			return BAN::Error::from_errno(EINVAL);

		const auto format = static_cast<FormatCode>(format_chunk->wFormatTag);
		const uint16_t bps = format_chunk->wBitsPerSample;
		const uint16_t channels = format_chunk->nChannels;

		if (channels == 0)
			return BAN::Error::from_errno(EINVAL);

		switch (format)
		{
			case FormatCode::WAVE_FORMAT_PCM:
				if (bps != 8 && bps != 16 && bps != 32)
					return BAN::Error::from_errno(ENOTSUP);
				break;
			case FormatCode::WAVE_FORMAT_IEEE_FLOAT:
				if (bps != 32 && bps != 64)
					return BAN::Error::from_errno(ENOTSUP);
				break;
			default:
				return BAN::Error::from_errno(ENOTSUP);
		}

		if (bps / 8 * channels != format_chunk->nBlockAlign)
			return BAN::Error::from_errno(EINVAL);

		auto loader = TRY(BAN::UniqPtr<WAVAudioLoader>::create());
		loader->m_bits_per_sample = bps;
		loader->m_sample_format = format;
		loader->m_channels = channels;
		loader->m_sample_rate = format_chunk->nSamplePerSec;
		loader->m_sample_data = sample_data;
		loader->m_total_samples = sample_data.size() / (bps / 8);
		loader->m_current_sample = 0;
		return BAN::UniqPtr<AudioLoader>(BAN::move(loader));
	}

	template<typename T>
	static double read_sample(BAN::ConstByteSpan data)
	{
		if constexpr(BAN::is_same_v<float, T> || BAN::is_same_v<double, T>)
			return data.as<const T>();
		else if constexpr(BAN::is_signed_v<T>)
			return data.as<const T>() / static_cast<double>(BAN::numeric_limits<T>::max());
		else
			return data.as<const T>() / (BAN::numeric_limits<T>::max() / 2.0) - 1.0;
	}

	double WAVAudioLoader::get_sample()
	{
		ASSERT(samples_remaining() > 0);

		const auto current_sample_data = m_sample_data.slice((m_bits_per_sample / 8) * m_current_sample++);

		switch (m_sample_format)
		{
			case FormatCode::WAVE_FORMAT_PCM:
				switch (m_bits_per_sample)
				{
					case 8:  return read_sample<uint8_t>(current_sample_data);
					case 16: return read_sample<int16_t>(current_sample_data);
					case 32: return read_sample<int32_t>(current_sample_data);
				}
				break;
			case FormatCode::WAVE_FORMAT_IEEE_FLOAT:
				switch (m_bits_per_sample)
				{
					case 32: return read_sample<float>(current_sample_data);
					case 64: return read_sample<double>(current_sample_data);
				}
				break;
		}

		ASSERT_NOT_REACHED();
	}

}
