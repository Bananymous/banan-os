#pragma once

#include <kernel/FS/Inode.h>
#include <kernel/Lock/Mutex.h>
#include <kernel/Memory/ByteRingBuffer.h>
#include <kernel/ThreadBlocker.h>

namespace Kernel
{

	class Pipe : public Inode
	{
	public:
		static BAN::ErrorOr<BAN::RefPtr<Inode>> create(const Credentials&);

		void on_close(int status_flags) override;
		void on_clone(int status_flags) override;

		virtual const FileSystem* filesystem() const override { return nullptr; }

	protected:
		virtual BAN::ErrorOr<size_t> read_impl(off_t, BAN::ByteSpan) override;
		virtual BAN::ErrorOr<size_t> write_impl(off_t, BAN::ConstByteSpan) override;
		virtual BAN::ErrorOr<void> fsync_impl() final override { return {}; }

		virtual bool can_read_impl() const override { return !m_buffer->empty(); }
		virtual bool can_write_impl() const override { return true; }
		virtual bool has_error_impl() const override { return m_reading_count == 0; }
		virtual bool has_hungup_impl() const override { return m_writing_count == 0; }

		virtual BAN::ErrorOr<long> ioctl_impl(int, void*) override;

	private:
		Pipe(const Credentials&);

	private:
		Mutex m_mutex;
		ThreadBlocker m_thread_blocker;

		BAN::UniqPtr<ByteRingBuffer> m_buffer;

		BAN::Atomic<uint32_t> m_writing_count { 1 };
		BAN::Atomic<uint32_t> m_reading_count { 1 };
	};

}
