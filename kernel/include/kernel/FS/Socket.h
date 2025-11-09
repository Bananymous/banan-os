#pragma once

#include <kernel/FS/Inode.h>

namespace Kernel
{

	class Socket : public Inode
	{
	public:
		enum class Domain
		{
			INET,
			INET6,
			UNIX,
		};

		enum class Type
		{
			STREAM,
			DGRAM,
			SEQPACKET,
		};

		struct Info
		{
			mode_t mode;
			uid_t uid;
			gid_t gid;
		};

	public:
		ino_t		ino()		const final override { ASSERT_NOT_REACHED(); }
		Mode		mode()		const final override { return Mode(m_info.mode); }
		nlink_t		nlink()		const final override { ASSERT_NOT_REACHED(); }
		uid_t		uid()		const final override { return m_info.uid; }
		gid_t		gid()		const final override { return m_info.gid; }
		off_t		size()		const final override { ASSERT_NOT_REACHED(); }
		timespec	atime()		const final override { ASSERT_NOT_REACHED(); }
		timespec	mtime()		const final override { ASSERT_NOT_REACHED(); }
		timespec	ctime()		const final override { ASSERT_NOT_REACHED(); }
		blksize_t	blksize()	const final override { ASSERT_NOT_REACHED(); }
		blkcnt_t	blocks()	const final override { ASSERT_NOT_REACHED(); }
		dev_t		dev()		const final override { ASSERT_NOT_REACHED(); }
		dev_t		rdev()		const final override { ASSERT_NOT_REACHED(); }

		const FileSystem* filesystem() const final override { return nullptr; }

	protected:
		Socket(const Info& info)
			: m_info(info)
		{}

		BAN::ErrorOr<void> fsync_impl() final override { return {}; }

	private:
		const Info m_info;
	};

}
