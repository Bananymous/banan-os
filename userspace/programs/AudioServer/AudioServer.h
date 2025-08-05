#pragma once

#include <BAN/Array.h>
#include <BAN/ByteSpan.h>
#include <BAN/CircularQueue.h>
#include <BAN/HashMap.h>

#include <LibAudio/Audio.h>

class AudioServer
{
	BAN_NON_MOVABLE(AudioServer);
	BAN_NON_COPYABLE(AudioServer);

public:
	AudioServer(int audio_device_fd);

	BAN::ErrorOr<void> on_new_client(int fd);
	void on_client_disconnect(int fd);
	bool on_client_packet(int fd, long smo_key);

	uint64_t update();

private:
	struct ClientInfo
	{
		LibAudio::AudioBuffer* buffer;
		size_t sample_frames_queued { 0 };
	};

	using sample_t = LibAudio::AudioBuffer::sample_t;

private:
	enum class AddOrRemove { Add, Remove };

	void reset_kernel_buffer();

	void send_samples();

private:
	const int m_audio_device_fd;
	uint32_t m_sample_rate;
	uint32_t m_channels;

	size_t m_samples_sent { 0 };
	BAN::Array<uint8_t, 1024> m_send_buffer;
	BAN::CircularQueue<sample_t, 64 * 1024> m_samples;

	BAN::HashMap<int, ClientInfo> m_audio_buffers;
};
