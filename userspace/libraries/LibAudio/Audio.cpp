#include <BAN/ScopeGuard.h>

#include <LibAudio/Audio.h>
#include <LibAudio/Protocol.h>

#include <fcntl.h>
#include <stdlib.h>
#include <sys/shm.h>
#include <sys/socket.h>
#include <sys/un.h>

namespace LibAudio
{

	BAN::ErrorOr<Audio> Audio::create(uint32_t channels, uint32_t sample_rate, uint32_t sample_frames)
	{
		Audio result;
		TRY(result.initialize((sample_frames + 10) * channels));

		result.m_audio_buffer->sample_rate = sample_rate;
		result.m_audio_buffer->channels = channels;

		return result;
	}

	BAN::ErrorOr<Audio> Audio::load(BAN::StringView path)
	{
		Audio result(TRY(AudioLoader::load(path)));
		TRY(result.initialize(256 * 1024));
		return result;
	}

	BAN::ErrorOr<Audio> Audio::random(uint32_t samples)
	{
		Audio result;
		TRY(result.initialize(samples));

		result.m_audio_buffer->sample_rate = 48000;
		result.m_audio_buffer->channels = 1;

		for (size_t i = 0; i < samples - 1; i++)
			result.m_audio_buffer->samples[i] = (rand() - RAND_MAX / 2) / (RAND_MAX / 2.0);
		result.m_audio_buffer->head = samples - 1;

		return result;
	}

	void Audio::clear()
	{
		if (m_audio_buffer)
			shmdt(m_audio_buffer);
		m_audio_buffer = nullptr;

		m_shmid = -1;

		if (m_server_fd != -1)
			close(m_server_fd);
		m_server_fd = -1;

		m_audio_loader.clear();
	}

	Audio& Audio::operator=(Audio&& other)
	{
		clear();

		m_server_fd    = other.m_server_fd;
		m_shmid        = other.m_shmid;
		m_audio_buffer = other.m_audio_buffer;
		m_audio_loader = BAN::move(other.m_audio_loader);

		other.m_server_fd    = -1;
		other.m_shmid        = -1;
		other.m_audio_buffer = nullptr;

		return *this;
	}

	BAN::ErrorOr<void> Audio::initialize(uint32_t total_samples)
	{
		const size_t shm_size = sizeof(AudioBuffer) + total_samples * sizeof(AudioBuffer::sample_t);

		m_shmid = shmget(IPC_PRIVATE, shm_size, 0666);
		if (m_shmid == -1)
			return BAN::Error::from_errno(errno);

		m_audio_buffer = static_cast<AudioBuffer*>(shmat(m_shmid, nullptr, 0));
		shmctl(m_shmid, IPC_RMID, nullptr);

		if (m_audio_buffer == SHM_FAILED)
			return BAN::Error::from_errno(errno);
		new (m_audio_buffer) AudioBuffer();
		memset(m_audio_buffer->samples, 0, total_samples * sizeof(AudioBuffer::sample_t));

		m_audio_buffer->capacity = total_samples;
		if (m_audio_loader)
		{
			m_audio_buffer->channels = m_audio_loader->channels();
			m_audio_buffer->sample_rate = m_audio_loader->sample_rate();
		}

		update();

		sockaddr_un server_addr;
		server_addr.sun_family = AF_UNIX;
		strcpy(server_addr.sun_path, s_audio_server_socket.data());

		m_server_fd = socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
		if (m_server_fd == -1)
			return BAN::Error::from_errno(errno);

		if (connect(m_server_fd, reinterpret_cast<sockaddr*>(&server_addr), sizeof(server_addr)) == -1)
			return BAN::Error::from_errno(errno);

		return {};
	}

	BAN::ErrorOr<void> Audio::start()
	{
		ASSERT(m_server_fd != -1);

		const LibAudio::Packet packet {
			.type = LibAudio::Packet::RegisterBuffer,
			.parameter = static_cast<uint64_t>(m_shmid),
		};

		const ssize_t nsend = send(m_server_fd, &packet, sizeof(packet), 0);
		if (nsend == -1)
			return BAN::Error::from_errno(errno);
		ASSERT(nsend == sizeof(packet));

		return {};
	}

	void Audio::set_paused(bool paused)
	{
		ASSERT(m_server_fd != -1);

		if (m_audio_buffer->paused == paused)
			return;
		m_audio_buffer->paused = paused;

		const LibAudio::Packet packet {
			.type = LibAudio::Packet::Notify,
			.parameter = {},
		};

		send(m_server_fd, &packet, sizeof(packet), 0);
	}

	size_t Audio::queueable_samples() const
	{
		const uint32_t server_samples = (m_audio_buffer->capacity + m_audio_buffer->head - m_audio_buffer->tail) % m_audio_buffer->capacity;
		return m_audio_buffer->capacity - server_samples - 1;
	}

	size_t Audio::queue_samples(BAN::Span<const AudioBuffer::sample_t> samples)
	{
		const uint32_t sample_count = BAN::Math::min<uint32_t>(queueable_samples(), samples.size());

		const uint32_t head     = m_audio_buffer->head;
		const uint32_t capacity = m_audio_buffer->capacity;

		for (size_t i = 0; i < sample_count; i++)
			m_audio_buffer->samples[(head + i) % capacity] = samples[i];
		m_audio_buffer->head = (head + sample_count) % capacity;

		return sample_count;
	}

	void Audio::update()
	{
		if (!m_audio_loader)
			return;

		if (!m_audio_loader->samples_remaining() && !is_playing())
			return set_paused(true);

		for (;;)
		{
			const uint32_t sample_count = BAN::Math::min<uint32_t>(queueable_samples(), m_audio_loader->samples_remaining());
			if (sample_count == 0)
				break;

			const uint32_t head     = m_audio_buffer->head;
			const uint32_t capacity = m_audio_buffer->capacity;

			for (size_t i = 0; i < sample_count; i++)
				m_audio_buffer->samples[(head + i) % capacity] = m_audio_loader->get_sample();
			m_audio_buffer->head = (head + sample_count) % capacity;
		}
	}

}
