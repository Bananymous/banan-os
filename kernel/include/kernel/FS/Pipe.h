#pragma once

#include <BAN/Array.h>
#include <kernel/FS/Inode.h>
#include <kernel/ThreadBlocker.h>

namespace Kernel
{

	class Pipe : public Inode
	{
	public:
		static BAN::ErrorOr<BAN::RefPtr<Inode>> create(const Credentials&);

		virtual bool is_pipe() const override { return true; }
		void clone_writing();
		void close_writing();

		virtual ino_t ino() const override { return 0; } // FIXME
		virtual Mode mode() const override { return { Mode::IFIFO | Mode::IRUSR | Mode::IWUSR }; }
		virtual nlink_t nlink() const override { return 1; }
		virtual uid_t uid() const override { return m_uid; }
		virtual gid_t gid() const override { return m_gid; }
		virtual off_t size() const override { return 0; }
		virtual timespec atime() const override { return m_atime; }
		virtual timespec mtime() const override { return m_mtime; }
		virtual timespec ctime() const override { return m_ctime; }
		virtual blksize_t blksize() const override { return 4096; }
		virtual blkcnt_t blocks() const override { return 0; }
		virtual dev_t dev() const override { return 0; } // FIXME
		virtual dev_t rdev() const override { return 0; } // FIXME

	protected:
		virtual BAN::ErrorOr<size_t> read_impl(off_t, BAN::ByteSpan) override;
		virtual BAN::ErrorOr<size_t> write_impl(off_t, BAN::ConstByteSpan) override;
		virtual BAN::ErrorOr<void> fsync_impl() final override { return {}; }

		virtual bool can_read_impl() const override { return m_buffer_size > 0; }
		virtual bool can_write_impl() const override { return true; }
		virtual bool has_error_impl() const override { return false; }

	private:
		Pipe(const Credentials&);

	private:
		const uid_t m_uid;
		const gid_t m_gid;
		timespec m_atime {};
		timespec m_mtime {};
		timespec m_ctime {};
		ThreadBlocker m_thread_blocker;

		BAN::Array<uint8_t, PAGE_SIZE> m_buffer;
		BAN::Atomic<size_t> m_buffer_size { 0 };
		size_t m_buffer_tail { 0 };

		BAN::Atomic<uint32_t> m_writing_count { 1 };
	};

}
