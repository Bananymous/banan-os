#include <kernel/Credentials.h>
#include <kernel/FS/VirtualFileSystem.h>

namespace Kernel
{

	bool Credentials::has_egid(gid_t gid) const
	{
		if (m_egid == gid)
			return true;
		for (gid_t supplementary : m_supplementary)
			if (gid == supplementary)
				return true;
		return false;
	}

	BAN::ErrorOr<void> Credentials::set_groups(BAN::Span<const gid_t> groups)
	{
		m_supplementary.clear();
		TRY(m_supplementary.resize(groups.size()));
		for (size_t i = 0; i < groups.size(); i++)
			m_supplementary[i] = groups[i];
		return {};
	}

}
