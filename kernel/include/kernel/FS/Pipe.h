#pragma once

#include <kernel/FS/Inode.h>
#include <kernel/Semaphore.h>
#include <kernel/SpinLock.h>

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

	private:
		Pipe(const Credentials&);

	private:
		const uid_t m_uid;
		const gid_t m_gid;
		timespec m_atime {};
		timespec m_mtime {};
		timespec m_ctime {};
		BAN::Vector<uint8_t> m_buffer;
		SpinLock m_lock;
		Semaphore m_semaphore;

		uint32_t m_writing_count { 1 };
	};

}
