#pragma once

#include <kernel/FS/TmpFS/FileSystem.h>
#include <kernel/FS/TmpFS/Inode.h>
#include <kernel/Process.h>

namespace Kernel
{

	class ProcPidInode final : public TmpDirectoryInode
	{
	public:
		static BAN::ErrorOr<BAN::RefPtr<ProcPidInode>> create_new(Process&, TmpFileSystem&, mode_t);
		~ProcPidInode() = default;

		virtual uid_t uid() const override { return m_process.credentials().ruid(); }
		virtual gid_t gid() const override { return m_process.credentials().rgid(); }

		void cleanup();

	protected:
		virtual BAN::ErrorOr<void> unlink_impl(BAN::StringView) override { return BAN::Error::from_errno(EPERM); }

	private:
		ProcPidInode(Process&, TmpFileSystem&, const TmpInodeInfo&);

	private:
		Process& m_process;
	};

	class ProcROProcessInode final : public TmpInode
	{
	public:
		static BAN::ErrorOr<BAN::RefPtr<ProcROProcessInode>> create_new(Process&, size_t (Process::*callback)(off_t, BAN::ByteSpan) const, TmpFileSystem&, mode_t);
		~ProcROProcessInode() = default;

		virtual uid_t uid() const override { return m_process.credentials().ruid(); }
		virtual gid_t gid() const override { return m_process.credentials().rgid(); }

	protected:
		virtual BAN::ErrorOr<size_t> read_impl(off_t, BAN::ByteSpan) override;

		// You may not write here and this is always non blocking
		virtual BAN::ErrorOr<size_t> write_impl(off_t, BAN::ConstByteSpan) override		{ return BAN::Error::from_errno(EINVAL); }
		virtual BAN::ErrorOr<void> truncate_impl(size_t) override						{ return BAN::Error::from_errno(EINVAL); }

		virtual bool can_read_impl() const override { return true; }
		virtual bool can_write_impl() const override { return false; }
		virtual bool has_error_impl() const override { return false; }
		virtual bool has_hangup_impl() const override { return false; }

	private:
		ProcROProcessInode(Process&, size_t (Process::*)(off_t, BAN::ByteSpan) const, TmpFileSystem&, const TmpInodeInfo&);

	private:
		Process& m_process;
		size_t (Process::*m_callback)(off_t, BAN::ByteSpan) const;
	};

	class ProcROInode final : public TmpInode
	{
	public:
		static BAN::ErrorOr<BAN::RefPtr<ProcROInode>> create_new(size_t (*callback)(off_t, BAN::ByteSpan), TmpFileSystem&, mode_t, uid_t, gid_t);
		~ProcROInode() = default;

	protected:
		virtual BAN::ErrorOr<size_t> read_impl(off_t, BAN::ByteSpan) override;

		// You may not write here and this is always non blocking
		virtual BAN::ErrorOr<size_t> write_impl(off_t, BAN::ConstByteSpan) override		{ return BAN::Error::from_errno(EINVAL); }
		virtual BAN::ErrorOr<void> truncate_impl(size_t) override						{ return BAN::Error::from_errno(EINVAL); }

		virtual bool can_read_impl() const override { return true; }
		virtual bool can_write_impl() const override { return false; }
		virtual bool has_error_impl() const override { return false; }
		virtual bool has_hangup_impl() const override { return false; }

	private:
		ProcROInode(size_t (*callback)(off_t, BAN::ByteSpan), TmpFileSystem&, const TmpInodeInfo&);

	private:
		size_t (*m_callback)(off_t, BAN::ByteSpan);
	};

}
