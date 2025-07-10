#include <BAN/ScopeGuard.h>

#include <LibAudio/Audio.h>

#include <fcntl.h>
#include <stdlib.h>
#include <sys/banan-os.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/un.h>

namespace LibAudio
{

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
			munmap(m_audio_buffer, m_smo_size);
		m_audio_buffer = nullptr;

		if (m_smo_key != -1)
			smo_delete(m_smo_key);
		m_smo_key = -1;

		if (m_server_fd != -1)
			close(m_server_fd);
		m_server_fd = -1;

		m_audio_loader.clear();
	}

	Audio& Audio::operator=(Audio&& other)
	{
		clear();

		m_server_fd    = other.m_server_fd;
		m_smo_key      = other.m_smo_key;
		m_smo_size     = other.m_smo_size;
		m_audio_buffer = other.m_audio_buffer;
		m_audio_loader = BAN::move(other.m_audio_loader);

		other.m_server_fd    = -1;
		other.m_smo_key      = -1;
		other.m_smo_size     = 0;
		other.m_audio_buffer = nullptr;

		return *this;
	}

	BAN::ErrorOr<void> Audio::start()
	{
		ASSERT(m_server_fd != -1);

		const ssize_t nsend = send(m_server_fd, &m_smo_key, sizeof(m_smo_key), 0);
		if (nsend == -1)
			return BAN::Error::from_errno(errno);
		ASSERT(nsend == sizeof(m_smo_key));

		return {};
	}

	BAN::ErrorOr<void> Audio::initialize(uint32_t total_samples)
	{
		m_smo_size = sizeof(AudioBuffer) + total_samples * sizeof(AudioBuffer::sample_t);

		m_smo_key = smo_create(m_smo_size, PROT_READ | PROT_WRITE);
		if (m_smo_key == -1)
			return BAN::Error::from_errno(errno);

		m_audio_buffer = static_cast<AudioBuffer*>(smo_map(m_smo_key));
		if (m_audio_buffer == nullptr)
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

	void Audio::update()
	{
		if (!m_audio_loader)
			return;

		while (m_audio_loader->samples_remaining())
		{
			const uint32_t next_head = (m_audio_buffer->head + 1) % m_audio_buffer->capacity;
			if (next_head == m_audio_buffer->tail)
				break;
			m_audio_buffer->samples[m_audio_buffer->head] = m_audio_loader->get_sample();
			m_audio_buffer->head = next_head;
		}
	}

}
