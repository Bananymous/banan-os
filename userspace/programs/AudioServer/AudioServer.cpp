#include "AudioServer.h"

#include <sys/banan-os.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <unistd.h>

AudioServer::AudioServer(BAN::Vector<AudioDevice>&& audio_devices)
	: m_audio_devices(BAN::move(audio_devices))
{
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
	update();
}

bool AudioServer::on_client_packet(int fd, LibAudio::Packet packet)
{
	auto& audio_buffer = m_audio_buffers[fd];

	BAN::Optional<uint32_t> response;

	switch (packet.type)
	{
		case LibAudio::Packet::Notify:
			if (audio_buffer.buffer == nullptr)
				break;
			reset_kernel_buffer();
			update();
			break;
		case LibAudio::Packet::RegisterBuffer:
			if (audio_buffer.buffer)
			{
				dwarnln("Client tried to map second audio buffer??");
				return false;
			}
			audio_buffer.buffer = static_cast<LibAudio::AudioBuffer*>(smo_map(packet.parameter));
			audio_buffer.queued_head = audio_buffer.buffer->tail;
			if (audio_buffer.buffer == nullptr)
			{
				dwarnln("Failed to map audio buffer: {}", strerror(errno));
				return false;
			}
			reset_kernel_buffer();
			update();
			break;
		case LibAudio::Packet::QueryDevices:
			response = m_audio_devices.size();
			break;
		case LibAudio::Packet::QueryPins:
			response = device().total_pins;
			break;
		case LibAudio::Packet::GetDevice:
			response = m_current_audio_device;
			break;
		case LibAudio::Packet::SetDevice:
			if (packet.parameter >= m_audio_devices.size())
			{
				dwarnln("Client tried to set device {} while there are only {}", packet.parameter, m_audio_devices.size());
				return false;
			}
			reset_kernel_buffer();
			m_current_audio_device = packet.parameter;
			update();
			break;
		case LibAudio::Packet::GetPin:
			response = device().current_pin;
			break;
		case LibAudio::Packet::SetPin:
			if (packet.parameter >= device().total_pins)
			{
				dwarnln("Client tried to set pin {} while the device only has {}", packet.parameter, device().total_pins);
				return false;
			}
			reset_kernel_buffer();
			if (uint32_t pin = packet.parameter; ioctl(device().fd, SND_SET_PIN, &pin) != 0)
				dwarnln("Failed to set pin {}: {}", packet.parameter, strerror(errno));
			else
				device().current_pin = packet.parameter;
			update();
			break;
		default:
			dwarnln("unknown packet type {}", static_cast<uint8_t>(packet.type));
			return false;
	}

	if (response.has_value())
		if (send(fd, &response.value(), sizeof(uint32_t), 0) != sizeof(uint32_t))
			dwarnln("failed to respond to client :(");

	return true;
}

uint64_t AudioServer::update()
{
	// FIXME: get this from the kernel
	static constexpr uint64_t kernel_buffer_ms = 50;

	const auto& device = m_audio_devices[m_current_audio_device];

	uint32_t kernel_buffer_size;
	if (ioctl(device.fd, SND_GET_BUFFERSZ, &kernel_buffer_size) == -1)
		ASSERT_NOT_REACHED();

	const size_t kernel_samples = kernel_buffer_size / sizeof(int16_t);
	ASSERT(kernel_samples <= m_samples_sent);

	const uint32_t samples_played = m_samples_sent - kernel_samples;
	ASSERT(samples_played % device.channels == 0);

	const uint32_t sample_frames_played = samples_played / device.channels;

	for (uint32_t i = 0; i < samples_played; i++)
		m_samples.pop();
	m_samples_sent -= samples_played;

	const size_t max_sample_frames = (m_samples.capacity() - m_samples.size()) / device.channels;
	const size_t queued_samples_end = m_samples.size();
	if (max_sample_frames == 0)
		return kernel_buffer_ms;

	size_t max_sample_frames_to_queue = max_sample_frames;

	bool anyone_playing = false;
	for (auto& [_, buffer] : m_audio_buffers)
	{
		if (buffer.buffer == nullptr)
			continue;

		if (const size_t sample_frames_queued = buffer.sample_frames_queued())
		{
			const sample_t sample_ratio = buffer.buffer->sample_rate / static_cast<sample_t>(device.sample_rate);
			const uint32_t buffer_sample_frames_played = BAN::Math::min<size_t>(
				BAN::Math::ceil(sample_frames_played * sample_ratio),
				sample_frames_queued
			);
			buffer.buffer->tail = (buffer.buffer->tail + buffer_sample_frames_played * buffer.buffer->channels) % buffer.buffer->capacity;
		}

		if (buffer.buffer->paused)
			continue;
		anyone_playing = true;

		max_sample_frames_to_queue = BAN::Math::min<size_t>(max_sample_frames_to_queue, buffer.sample_frames_available());
	}

	if (!anyone_playing)
		return 60'000;

	const uint32_t sample_frames_per_10ms = device.sample_rate / 100;
	if (max_sample_frames_to_queue < sample_frames_per_10ms)
	{
		const uint32_t sample_frames_sent = m_samples_sent / device.channels;
		if (sample_frames_sent >= sample_frames_per_10ms)
			return 1;
		max_sample_frames_to_queue = sample_frames_per_10ms;
	}

	for (auto& [_, buffer] : m_audio_buffers)
	{
		if (buffer.buffer == nullptr || buffer.buffer->paused)
			continue;

		const sample_t sample_ratio = buffer.buffer->sample_rate / static_cast<sample_t>(device.sample_rate);

		const size_t sample_frames_to_queue = BAN::Math::min<size_t>(
			BAN::Math::ceil(buffer.sample_frames_available() / sample_ratio),
			max_sample_frames_to_queue
		);
		if (sample_frames_to_queue == 0)
			continue;

		while (m_samples.size() < queued_samples_end + sample_frames_to_queue * device.channels)
			m_samples.push(0.0);

		const size_t min_channels = BAN::Math::min(device.channels, buffer.buffer->channels);

		const size_t buffer_tail = buffer.queued_head;
		for (size_t i = 0; i < sample_frames_to_queue; i++)
		{
			const size_t buffer_frame = i * sample_ratio;
			for (size_t j = 0; j < min_channels; j++)
				m_samples[queued_samples_end + i * device.channels + j] += buffer.buffer->samples[(buffer_tail + buffer_frame * buffer.buffer->channels + j) % buffer.buffer->capacity];
		}

		const uint32_t buffer_sample_frames_queued = BAN::Math::min<uint32_t>(
			BAN::Math::ceil(sample_frames_to_queue * sample_ratio),
			buffer.sample_frames_available()
		);
		buffer.queued_head = (buffer_tail + buffer_sample_frames_queued * buffer.buffer->channels) % buffer.buffer->capacity;
	}

	send_samples();

	const double play_ms = 1000.0 * m_samples_sent / device.channels / device.sample_rate;
	const uint64_t wake_ms = BAN::Math::max<uint64_t>(play_ms, kernel_buffer_ms) - kernel_buffer_ms;
	return wake_ms;
}

void AudioServer::reset_kernel_buffer()
{
	const auto& device = m_audio_devices[m_current_audio_device];

	uint32_t kernel_buffer_size;
	if (ioctl(device.fd, SND_RESET_BUFFER, &kernel_buffer_size) != 0)
		ASSERT_NOT_REACHED();

	const size_t kernel_samples = kernel_buffer_size / sizeof(int16_t);
	ASSERT(kernel_samples <= m_samples_sent);

	const uint32_t samples_played = m_samples_sent - kernel_samples;
	ASSERT(samples_played % device.channels == 0);

	const uint32_t sample_frames_played = samples_played / device.channels;

	m_samples_sent = 0;
	m_samples.clear();

	for (auto& [_, buffer] : m_audio_buffers)
	{
		if (buffer.buffer == nullptr)
			continue;

		if (const size_t sample_frames_queued = buffer.sample_frames_queued())
		{
			const sample_t sample_ratio = buffer.buffer->sample_rate / static_cast<sample_t>(device.sample_rate);
			const uint32_t buffer_sample_frames_played = BAN::Math::min<size_t>(
				BAN::Math::ceil(sample_frames_played * sample_ratio),
				sample_frames_queued
			);
			buffer.buffer->tail = (buffer.buffer->tail + buffer_sample_frames_played * buffer.buffer->channels) % buffer.buffer->capacity;
			buffer.queued_head = buffer.buffer->tail;
		}
	}
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
				sample_buffer[i] = BAN::Math::clamp<sample_t>(
					0.2 * m_samples[m_samples_sent + i] * BAN::numeric_limits<kernel_sample_t>::max(),
					BAN::numeric_limits<kernel_sample_t>::min(),
					BAN::numeric_limits<kernel_sample_t>::max()
				);
			}
		}

		size_t nwritten = 0;
		while (nwritten < buffer.size())
		{
			const ssize_t nwrite = write(
				device().fd,
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
