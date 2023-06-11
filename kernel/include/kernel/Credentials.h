#pragma once

#include <sys/types.h>

namespace Kernel
{

	class Credentials
	{
	public:
		Credentials(uid_t ruid, uid_t euid, gid_t rgid, gid_t egid)
			: m_ruid(ruid), m_euid(euid), m_suid(0)
			, m_rgid(rgid), m_egid(egid), m_sgid(0)
		{ }

		uid_t ruid() const { return m_ruid; }
		uid_t euid() const { return m_euid; }
		uid_t suid() const { return m_suid; }

		gid_t rgid() const { return m_rgid; }
		gid_t egid() const { return m_egid; }
		gid_t sgid() const { return m_sgid; }

	private:
		uid_t m_ruid, m_euid, m_suid;
		gid_t m_rgid, m_egid, m_sgid;
	};

}

namespace BAN::Formatter
{

	template<typename F>
	void print_argument(F putc, const Kernel::Credentials& credentials, const ValueFormat&)
	{
		print(putc, "(ruid {}, euid {})", credentials.ruid(), credentials.euid());
	}

}
