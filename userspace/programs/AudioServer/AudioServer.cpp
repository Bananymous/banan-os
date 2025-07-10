#include "AudioServer.h"

#include <sys/banan-os.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>

AudioServer::AudioServer(int fd)
	: m_audio_device_fd(fd)
{
	if (ioctl(m_audio_device_fd, SND_GET_CHANNELS, &m_channels) != 0)
		ASSERT_NOT_REACHED();
	if (ioctl(m_audio_device_fd, SND_GET_SAMPLE_RATE, &m_sample_rate) != 0)
		ASSERT_NOT_REACHED();
}

BAN::ErrorOr<void> AudioServer::on_new_client(int fd)
{
	TRY(m_audio_buffers.emplace(fd, nullptr));
	return {};
}

void AudioServer::on_client_disconnect(int fd)
{
	auto it = m_audio_buffers.find(fd);
	ASSERT(it != m_audio_buffers.end());

	if (it->value.buffer != nullptr)
	{
		const size_t bytes = sizeof(LibAudio::AudioBuffer) + it->value.buffer->capacity * sizeof(LibAudio::AudioBuffer::sample_t);
		munmap(it->value.buffer, bytes);
	}

	m_audio_buffers.remove(it);

	reset_kernel_buffer();
}

bool AudioServer::on_client_packet(int fd, long smo_key)
{
	auto& audio_buffer = m_audio_buffers[fd];

	audio_buffer.buffer = static_cast<LibAudio::AudioBuffer*>(smo_map(smo_key));
	audio_buffer.sample_frames_queued = 0;
	if (audio_buffer.buffer == nullptr)
	{
		dwarnln("Failed to map audio buffer: {}", strerror(errno));
		return false;
	}

	reset_kernel_buffer();

	return true;
}

void AudioServer::update()
{
	uint32_t kernel_buffer_size;
	if (ioctl(m_audio_device_fd, SND_GET_BUFFERSZ, &kernel_buffer_size) == -1)
		ASSERT_NOT_REACHED();

	const size_t kernel_samples = kernel_buffer_size / sizeof(int16_t);
	ASSERT(kernel_samples <= m_samples_sent);

	const uint32_t samples_played = m_samples_sent - kernel_samples;
	ASSERT(samples_played % m_channels == 0);

	for (uint32_t i = 0; i < samples_played; i++)
		m_samples.pop();
	m_samples_sent -= samples_played;

	const size_t max_sample_frames = (m_samples.capacity() - m_samples.size()) / m_channels;
	const size_t queued_samples_end = m_samples.size();
	if (max_sample_frames == 0)
		return;

	for (auto& [_, buffer] : m_audio_buffers)
	{
		if (buffer.buffer == nullptr)
			continue;

		const double sample_ratio = buffer.buffer->sample_rate / static_cast<double>(m_sample_rate);

		if (buffer.sample_frames_queued)
		{
			const uint32_t buffer_sample_frames_played = BAN::Math::min<size_t>(
				samples_played * sample_ratio / m_channels,
				buffer.sample_frames_queued
			);
			buffer.buffer->tail = (buffer.buffer->tail + buffer_sample_frames_played * buffer.buffer->channels) % buffer.buffer->capacity;
			buffer.sample_frames_queued -= buffer_sample_frames_played;
		}

		const uint32_t buffer_total_sample_frames = ((buffer.buffer->capacity + buffer.buffer->head - buffer.buffer->tail) % buffer.buffer->capacity) / buffer.buffer->channels;
		const uint32_t buffer_sample_frames_available = (buffer_total_sample_frames - buffer.sample_frames_queued) / sample_ratio;
		if (buffer_sample_frames_available == 0)
			continue;

		const size_t sample_frames_to_queue = BAN::Math::min<size_t>(max_sample_frames, buffer_sample_frames_available);
		if (sample_frames_to_queue == 0)
			continue;

		while (m_samples.size() < queued_samples_end + sample_frames_to_queue * m_channels)
			m_samples.push(0.0);

		const size_t min_channels = BAN::Math::min(m_channels, buffer.buffer->channels);

		const size_t buffer_tail = buffer.buffer->tail + buffer.sample_frames_queued * buffer.buffer->channels;
		for (size_t i = 0; i < sample_frames_to_queue; i++)
		{
			const size_t buffer_frame = i * sample_ratio;
			for (size_t j = 0; j < min_channels; j++)
				m_samples[queued_samples_end + i * m_channels + j] += buffer.buffer->samples[(buffer_tail + buffer_frame * buffer.buffer->channels + j) % buffer.buffer->capacity];
		}

		buffer.sample_frames_queued += sample_frames_to_queue * sample_ratio;
	}

	send_samples();
}

void AudioServer::reset_kernel_buffer()
{
	uint32_t kernel_buffer_size;
	if (ioctl(m_audio_device_fd, SND_RESET_BUFFER, &kernel_buffer_size) != 0)
		ASSERT_NOT_REACHED();

	const size_t kernel_samples = kernel_buffer_size / sizeof(int16_t);
	ASSERT(kernel_samples <= m_samples_sent);

	const uint32_t samples_played = m_samples_sent - kernel_samples;
	ASSERT(samples_played % m_channels == 0);

	m_samples_sent = 0;
	m_samples.clear();

	for (auto& [_, buffer] : m_audio_buffers)
	{
		if (buffer.buffer == nullptr || buffer.sample_frames_queued == 0)
			continue;
		const uint32_t buffer_sample_frames_played = BAN::Math::min<size_t>(
			samples_played / m_channels,
			buffer.sample_frames_queued
		);
		buffer.buffer->tail = (buffer.buffer->tail + buffer_sample_frames_played * buffer.buffer->channels) % buffer.buffer->capacity;
		buffer.sample_frames_queued = 0;
	}

	update();
}

void AudioServer::send_samples()
{
	// FIXME: don't assume kernel uses 16 bit PCM
	using kernel_sample_t = int16_t;

	if (m_samples_sent >= m_samples.size())
		return;

	while (m_samples_sent < m_samples.size())
	{
		const size_t samples_to_send = BAN::Math::min(m_send_buffer.size() / sizeof(kernel_sample_t), m_samples.size() - m_samples_sent);

		auto buffer = BAN::ByteSpan(m_send_buffer.data(), samples_to_send * sizeof(kernel_sample_t));

		{
			auto sample_buffer = buffer.as_span<kernel_sample_t>();
			for (size_t i = 0; i < samples_to_send; i++)
			{
				sample_buffer[i] = BAN::Math::clamp<double>(
					m_samples[m_samples_sent + i] * BAN::numeric_limits<kernel_sample_t>::max(),
					BAN::numeric_limits<kernel_sample_t>::min(),
					BAN::numeric_limits<kernel_sample_t>::max()
				);
			}
		}

		size_t nwritten = 0;
		while (nwritten < buffer.size())
		{
			const ssize_t nwrite = write(
				m_audio_device_fd,
				buffer.data() + nwritten,
				buffer.size() - nwritten
			);
			if (nwrite == -1)
			{
				if (errno != EAGAIN && errno != EWOULDBLOCK)
					dwarnln("write: {}", strerror(errno));
				break;
			}
			nwritten += nwrite;
		}

		m_samples_sent += nwritten / sizeof(kernel_sample_t);

		if (nwritten < buffer.size())
			break;
	}
}
