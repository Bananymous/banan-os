#pragma once

#include <kernel/FS/Inode.h>
#include <kernel/Lock/Mutex.h>
#include <kernel/ThreadBlocker.h>

namespace Kernel
{

	class EventFD final : public Inode
	{
	public:
		static BAN::ErrorOr<BAN::RefPtr<Inode>> create(uint64_t initval, bool semaphore);

	private:
		EventFD(uint64_t initval, bool is_semaphore);

		const FileSystem* filesystem() const override { return nullptr; }

		BAN::ErrorOr<void> sync_inode(SyncType) override { return {}; }
		BAN::ErrorOr<void> sync_data() override { return {}; }

		BAN::ErrorOr<size_t> read_impl(off_t, BAN::ByteSpan) override;
		BAN::ErrorOr<size_t> write_impl(off_t, BAN::ConstByteSpan) override;

		bool can_read_impl() const override { return m_value > 0; }
		bool can_write_impl() const override { return m_value < UINT64_MAX - 1; }
		bool has_error_impl() const override { return false; }
		bool has_hungup_impl() const override { return false; }

	private:
		const bool m_is_semaphore;
		BAN::Atomic<uint64_t> m_value;

		Mutex m_mutex;
		ThreadBlocker m_thread_blocker;
	};

}
