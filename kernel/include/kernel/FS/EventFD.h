#pragma once

#include <kernel/FS/Inode.h>

namespace Kernel
{

	class EventFD final : public Inode
	{
	public:
		static BAN::ErrorOr<BAN::RefPtr<Inode>> create(uint64_t initval, bool semaphore);

		ino_t ino() const override { return 0; }
		Mode mode() const override { return { Mode::IFCHR | Mode::IRUSR | Mode::IWUSR }; }
		nlink_t nlink() const override { return ref_count(); }
		uid_t uid() const override { return 0; }
		gid_t gid() const override { return 0; }
		off_t size() const override { return 0; }
		timespec atime() const override { return {}; }
		timespec mtime() const override { return {}; }
		timespec ctime() const override { return {}; }
		blksize_t blksize() const override { return 8; }
		blkcnt_t blocks() const override { return 0; }
		dev_t dev() const override { return 0; }
		dev_t rdev() const override { return 0; }

		const FileSystem* filesystem() const override { return nullptr; }

	protected:
		BAN::ErrorOr<size_t> read_impl(off_t, BAN::ByteSpan) override;
		BAN::ErrorOr<size_t> write_impl(off_t, BAN::ConstByteSpan) override;
		BAN::ErrorOr<void> fsync_impl() final override { return {}; }

		bool can_read_impl() const override { return m_value > 0; }
		bool can_write_impl() const override { return m_value < UINT64_MAX - 1; }
		bool has_error_impl() const override { return false; }
		bool has_hungup_impl() const override { return false; }

	private:
		EventFD(uint64_t initval, bool is_semaphore)
			: m_is_semaphore(is_semaphore)
			, m_value(initval)
		{ }

	private:
		const bool m_is_semaphore;
		uint64_t m_value;

		ThreadBlocker m_thread_blocker;
	};

}
