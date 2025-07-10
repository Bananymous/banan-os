#include <LibAudio/AudioLoader.h>
#include <LibAudio/AudioLoaders/WAVLoader.h>

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

namespace LibAudio
{

	BAN::ErrorOr<BAN::UniqPtr<AudioLoader>> AudioLoader::load(BAN::StringView path)
	{
		if (path.empty())
			return BAN::Error::from_errno(ENOENT);

		const bool malloced = (path.data()[path.size()] != '\0');

		const char* path_cstr = path.data();
		if (malloced && (path_cstr = strndup(path.data(), path.size())) == nullptr)
			return BAN::Error::from_errno(errno);
		const int file_fd = open(path_cstr, O_RDONLY);
		if (malloced)
			free(const_cast<char*>(path_cstr));
		if (file_fd == -1)
			return BAN::Error::from_errno(errno);

		struct stat st;
		if (fstat(file_fd, &st) == -1)
		{
			close(file_fd);
			return BAN::Error::from_errno(errno);
		}

		uint8_t* mmap_addr = static_cast<uint8_t*>(mmap(nullptr, st.st_size, PROT_READ, MAP_PRIVATE, file_fd, 0));
		close(file_fd);
		if (mmap_addr == MAP_FAILED)
			return BAN::Error::from_errno(errno);

		BAN::ErrorOr<BAN::UniqPtr<AudioLoader>> result_or_error { BAN::Error::from_errno(ENOTSUP) };

		auto file_span = BAN::ConstByteSpan(mmap_addr, st.st_size);
		if (WAVAudioLoader::can_load_from(file_span))
			result_or_error = WAVAudioLoader::create(file_span);

		if (result_or_error.is_error())
		{
			munmap(mmap_addr, st.st_size);
			return result_or_error.release_error();
		}

		auto result = result_or_error.release_value();
		result->m_mmap_addr = mmap_addr;
		result->m_mmap_size = st.st_size;
		return result;
	}

	AudioLoader::~AudioLoader()
	{
		if (m_mmap_addr)
			munmap(m_mmap_addr, m_mmap_size);
		m_mmap_addr = nullptr;
		m_mmap_size = 0;
	}

}
