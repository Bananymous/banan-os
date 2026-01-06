#pragma once

#include <BAN/Atomic.h>
#include <BAN/StringView.h>
#include <BAN/Vector.h>

#include <LibAudio/AudioLoader.h>

namespace LibAudio
{

	struct AudioBuffer
	{
		using sample_t = double;

		uint32_t sample_rate;
		uint32_t channels;

		BAN::Atomic<bool> paused { false };

		uint32_t capacity;
		BAN::Atomic<uint32_t> tail { 0 };
		BAN::Atomic<uint32_t> head { 0 };
		sample_t samples[/* capacity */];
	};

	class Audio
	{
		BAN_NON_COPYABLE(Audio);

	public:
		static BAN::ErrorOr<Audio> create(uint32_t channels, uint32_t sample_rate, uint32_t sample_frames);
		static BAN::ErrorOr<Audio> load(BAN::StringView path);
		static BAN::ErrorOr<Audio> random(uint32_t samples);
		~Audio() { clear(); }

		Audio(Audio&& other) { *this = BAN::move(other); }
		Audio& operator=(Audio&& other);

		BAN::ErrorOr<void> start();
		void update();

		void set_paused(bool paused);

		bool is_playing() const { return m_audio_buffer->tail != m_audio_buffer->head; }

		size_t queue_samples(BAN::Span<const AudioBuffer::sample_t> samples);

	private:
		Audio() = default;
		Audio(BAN::UniqPtr<AudioLoader>&& audio_loader)
			: m_audio_loader(BAN::move(audio_loader))
		{ }

		void clear();

		BAN::ErrorOr<void> initialize(uint32_t total_samples);

	private:
		int m_server_fd { -1 };

		BAN::UniqPtr<AudioLoader> m_audio_loader;

		long m_smo_key { -1 };
		size_t m_smo_size { 0 };
		AudioBuffer* m_audio_buffer { nullptr };
	};

}
