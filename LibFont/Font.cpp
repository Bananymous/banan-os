#include <BAN/ScopeGuard.h>

#include <LibFont/Font.h>
#include <LibFont/PSF.h>

#if __is_kernel
#include <kernel/FS/VirtualFileSystem.h>
#endif

#include <fcntl.h>

#if __is_kernel
extern uint8_t _binary_font_prefs_psf_start[];
extern uint8_t _binary_font_prefs_psf_end[];
#endif

namespace LibFont
{

#if __is_kernel
	BAN::ErrorOr<Font> Font::prefs()
	{
		size_t font_data_size = _binary_font_prefs_psf_end - _binary_font_prefs_psf_start;
		return load(BAN::ConstByteSpan(_binary_font_prefs_psf_start, font_data_size));
	}
#endif

	BAN::ErrorOr<Font> Font::load(BAN::StringView path)
	{
		BAN::Vector<uint8_t> file_data;

#if __is_kernel
		{
			auto inode = TRY(Kernel::VirtualFileSystem::get().file_from_absolute_path({ 0, 0, 0, 0 }, path, O_RDONLY)).inode;
			TRY(file_data.resize(inode->size()));
			TRY(inode->read(0, BAN::ByteSpan(file_data.span())));
		}
#else
		{
			char path_buffer[PATH_MAX];
			strncpy(path_buffer, path.data(), path.size());
			path_buffer[path.size()] = '\0';

			int fd = open(path_buffer, O_RDONLY);
			if (fd == -1)
				return BAN::Error::from_errno(errno);
			BAN::ScopeGuard file_closer([fd] { close(fd); });

			struct stat st;
			if (fstat(fd, &st) == -1)
				return BAN::Error::from_errno(errno);
			TRY(file_data.resize(st.st_size));

			ssize_t total_read = 0;
			while (total_read < st.st_size)
			{
				ssize_t nread = read(fd, file_data.data() + total_read, st.st_size - total_read);
				if (nread == -1)
					return BAN::Error::from_errno(errno);
				total_read += nread;
			}
		}
#endif

		return load(BAN::ConstByteSpan(file_data.span()));
	}

	BAN::ErrorOr<Font> Font::load(BAN::ConstByteSpan font_data)
	{
		if (is_psf1(font_data))
			return TRY(parse_psf1(font_data));
		if (is_psf2(font_data))
			return TRY(parse_psf2(font_data));
		return BAN::Error::from_errno(ENOTSUP);
	}

}
