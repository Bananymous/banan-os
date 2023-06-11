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

		void set_ruid(uid_t uid) { m_ruid = uid; }
		void set_euid(uid_t uid) { m_euid = uid; }
		void set_suid(uid_t uid) { m_suid = uid; }

		void set_rgid(gid_t gid) { m_rgid = gid; }
		void set_egid(gid_t gid) { m_egid = gid; }
		void set_sgid(gid_t gid) { m_sgid = gid; }

		bool is_superuser() const { return m_euid == 0; }

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
