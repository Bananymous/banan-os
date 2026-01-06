#pragma once

#include <BAN/Array.h>
#include <BAN/ByteSpan.h>
#include <BAN/CircularQueue.h>
#include <BAN/HashMap.h>

#include <LibAudio/Audio.h>
#include <LibAudio/Protocol.h>

struct AudioDevice
{
	int fd;
	uint32_t channels;
	uint32_t sample_rate;
	uint32_t total_pins;
	uint32_t current_pin;
};

class AudioServer
{
	BAN_NON_MOVABLE(AudioServer);
	BAN_NON_COPYABLE(AudioServer);

public:
	AudioServer(BAN::Vector<AudioDevice>&& audio_devices);

	BAN::ErrorOr<void> on_new_client(int fd);
	void on_client_disconnect(int fd);
	bool on_client_packet(int fd, LibAudio::Packet);

	uint64_t update();

private:
	AudioDevice& device() { return m_audio_devices[m_current_audio_device]; }

private:
	struct ClientInfo
	{
		LibAudio::AudioBuffer* buffer;
		size_t queued_head { 0 };

		size_t sample_frames_queued() const
		{
			return ((buffer->capacity + queued_head - buffer->tail) % buffer->capacity) / buffer->channels;
		}

		size_t sample_frames_available() const
		{
			return ((buffer->capacity + buffer->head - queued_head) % buffer->capacity) / buffer->channels;
		}
	};

	using sample_t = LibAudio::AudioBuffer::sample_t;

private:
	enum class AddOrRemove { Add, Remove };

	void reset_kernel_buffer();

	void send_samples();

private:
	BAN::Vector<AudioDevice> m_audio_devices;
	size_t m_current_audio_device { 0 };

	size_t m_samples_sent { 0 };
	BAN::Array<uint8_t, 4 * 1024> m_send_buffer;
	BAN::CircularQueue<sample_t, 64 * 1024> m_samples;

	BAN::HashMap<int, ClientInfo> m_audio_buffers;
};
