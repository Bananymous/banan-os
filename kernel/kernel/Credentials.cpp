#include <kernel/Credentials.h>
#include <kernel/FS/VirtualFileSystem.h>

#include <ctype.h>
#include <fcntl.h>

namespace Kernel
{

	static id_t parse_id(BAN::StringView line)
	{
		id_t id = 0;
		for (char c : line)
		{
			if (!isdigit(c))
				return -1;
			id = (id * 10) + (c - '0');
		}
		return id;
	};

	BAN::ErrorOr<BAN::String> Credentials::find_username() const
	{
		auto inode = TRY(VirtualFileSystem::get().file_from_absolute_path(*this, "/etc/passwd"_sv, O_RDONLY)).inode;

		BAN::String line;
		off_t offset = 0;
		uint8_t buffer[128];
		while (offset < inode->size())
		{
			size_t nread = TRY(inode->read(offset, { buffer, sizeof(buffer) }));

			bool line_done = false;
			for (size_t i = 0; i < nread; i++)
			{
				if (buffer[i] == '\n')
				{
					TRY(line.append({ (const char*)buffer, i }));
					line_done = true;
					offset += i + 1;
					break;
				}
			}
			if (!line_done)
			{
				offset += nread;
				TRY(line.append({ (const char*)buffer, nread }));
				continue;
			}

			auto parts = TRY(line.sv().split(':', true));
			if (parts.size() == 7 && m_euid == parse_id(parts[2]))
			{
				BAN::String result;
				TRY(result.append(parts[0]));
				return result;
			}

			line.clear();
		}

		auto parts = TRY(line.sv().split(':', true));
		if (parts.size() == 7 && m_euid == parse_id(parts[2]))
		{
			BAN::String result;
			TRY(result.append(parts[0]));
			return result;
		}

		return BAN::Error::from_errno(EINVAL);
	}

	BAN::ErrorOr<void> Credentials::initialize_supplementary_groups()
	{
		m_supplementary.clear();

		auto username = TRY(find_username());

		auto file_or_error = VirtualFileSystem::get().file_from_absolute_path(*this, "/etc/group", O_RDONLY);
		if (file_or_error.is_error())
		{
			if (file_or_error.error().get_error_code() == ENOENT)
				return {};
			return file_or_error.error();
		}

		auto inode = file_or_error.value().inode;

		BAN::String line;
		off_t offset = 0;
		uint8_t buffer[128];
		while (offset < inode->size())
		{
			size_t nread = TRY(inode->read(offset, { buffer, sizeof(buffer) }));

			bool line_done = false;
			for (size_t i = 0; i < nread; i++)
			{
				if (buffer[i] == '\n')
				{
					TRY(line.append({ (const char*)buffer, i }));
					line_done = true;
					offset += i + 1;
					break;
				}
			}
			if (!line_done)
			{
				offset += nread;
				TRY(line.append({ (const char*)buffer, nread }));
				continue;
			}

			auto parts = TRY(line.sv().split(':', true));
			if (parts.size() != 4)
			{
				line.clear();
				continue;
			}

			auto users = TRY(parts[3].split(','));
			for (auto user : users)
			{
				if (user != username)
					continue;
				if (gid_t gid = parse_id(parts[2]); gid != -1)
				{
					TRY(m_supplementary.push_back(gid));
					break;
				}
			}

			line.clear();
		}

		auto parts = TRY(line.sv().split(':', true));
		if (parts.size() == 4)
		{
			auto users = TRY(parts[3].split(','));
			for (auto user : users)
			{
				if (user != username)
					continue;
				if (gid_t gid = parse_id(parts[2]); gid != -1)
				{
					TRY(m_supplementary.push_back(gid));
					break;
				}
			}
		}

		return {};
	}

	bool Credentials::has_egid(gid_t gid) const
	{
		if (m_egid == gid)
			return true;
		for (gid_t supplementary : m_supplementary)
			if (gid == supplementary)
				return true;
		return false;
	}

}
