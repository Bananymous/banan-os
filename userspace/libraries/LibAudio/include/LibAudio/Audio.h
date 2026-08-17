#pragma once

#include <BAN/Atomic.h>
#include <BAN/StringView.h>
#include <BAN/Vector.h>

#include <LibAudio/AudioStream.h>

namespace LibAudio
{

	struct AudioBuffer
	{
		using sample_t = float;

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

		size_t queueable_samples() const;
		size_t queue_samples(BAN::Span<const AudioBuffer::sample_t> samples);

	private:
		Audio() = default;
		Audio(BAN::UniqPtr<AudioStream>&& audio_stream)
			: m_audio_stream(BAN::move(audio_stream))
		{ }

		void clear();

		BAN::ErrorOr<void> initialize(uint32_t total_samples);

	private:
		int m_server_fd { -1 };

		BAN::UniqPtr<AudioStream> m_audio_stream;

		int m_shmid { -1 };
		AudioBuffer* m_audio_buffer { nullptr };
	};

}
