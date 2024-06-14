#include <BAN/ScopeGuard.h>
#include <BAN/String.h>

#include <LibImage/Image.h>
#include <LibImage/Netbpm.h>

#include <fcntl.h>
#include <stdlib.h>
#include <sys/mman.h>

namespace LibImage
{

	BAN::ErrorOr<BAN::UniqPtr<Image>> Image::load_from_file(BAN::StringView path)
	{
		int fd = -1;

		if (path.data()[path.size()] == '\0')
		{
			fd = open(path.data(), O_RDONLY);
		}
		else
		{
			BAN::String path_str;
			TRY(path_str.append(path));
			fd = open(path_str.data(), O_RDONLY);
		}

		if (fd == -1)
		{
			fprintf(stddbg, "open: %s\n", strerror(errno));
			return BAN::Error::from_errno(errno);
		}

		BAN::ScopeGuard guard_file_close([fd] { close(fd); });

		struct stat st;
		if (fstat(fd, &st) == -1)
		{
			fprintf(stddbg, "fstat: %s\n", strerror(errno));
			return BAN::Error::from_errno(errno);
		}

		if (st.st_size < 2)
		{
			fprintf(stddbg, "invalid image (too small)\n");
			return BAN::Error::from_errno(EINVAL);
		}

		void* addr = mmap(nullptr, st.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
		if (addr == MAP_FAILED)
		{
			fprintf(stddbg, "mmap: %s\n", strerror(errno));
			return BAN::Error::from_errno(errno);
		}

		BAN::ScopeGuard guard_munmap([&] { munmap(addr, st.st_size); });

		auto image_data_span = BAN::ConstByteSpan(reinterpret_cast<uint8_t*>(addr), st.st_size);

		uint16_t u16_signature = image_data_span.as<const uint16_t>();
		switch (u16_signature)
		{
			case 0x3650:
			case 0x3550:
			case 0x3450:
			case 0x3350:
			case 0x3250:
			case 0x3150:
				return TRY(load_netbpm(image_data_span));
			default:
				fprintf(stderr, "unrecognized image format\n");
				break;
		}

		return BAN::Error::from_errno(ENOTSUP);
	}

}
