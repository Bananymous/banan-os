#pragma once

#include <kernel/FS/Inode.h>

#include <limits.h>

namespace Kernel
{

	class RamFileSystem;

	class RamInode : public Inode
	{
	public:
		static BAN::ErrorOr<BAN::RefPtr<RamInode>> create(RamFileSystem&, mode_t, uid_t, gid_t);
		virtual ~RamInode() = default;
	
		virtual ino_t		ino()		const override { return m_inode_info.ino; }
		virtual Mode		mode()		const override { return { m_inode_info.mode }; }
		virtual nlink_t		nlink()		const override { return m_inode_info.nlink; }
		virtual uid_t		uid()		const override { return m_inode_info.uid; }
		virtual gid_t		gid()		const override { return m_inode_info.gid; }
		virtual off_t		size()		const override { return m_inode_info.size; }
		virtual timespec	atime()		const override { return m_inode_info.atime; }
		virtual timespec	mtime()		const override { return m_inode_info.mtime; }
		virtual timespec	ctime()		const override { return m_inode_info.ctime; }
		virtual blksize_t	blksize()	const override { return m_inode_info.blksize; }
		virtual blkcnt_t	blocks()	const override { return m_inode_info.blocks; }
		virtual dev_t		dev()		const override { return m_inode_info.dev; }
		virtual dev_t		rdev()		const override { return m_inode_info.rdev; }

		virtual BAN::ErrorOr<size_t> read(size_t, void*, size_t) override;
		virtual BAN::ErrorOr<size_t> write(size_t, const void*, size_t)	override;

		virtual BAN::ErrorOr<void> truncate(size_t) override;

		void add_link() { m_inode_info.nlink++; }

	protected:
		RamInode(RamFileSystem& fs, mode_t, uid_t, gid_t);

	protected:
		struct FullInodeInfo
		{
			ino_t		ino;
			mode_t		mode;
			nlink_t		nlink;
			uid_t		uid;
			gid_t		gid;
			off_t		size;
			timespec	atime;
			timespec	mtime;
			timespec	ctime;
			blksize_t	blksize;
			blkcnt_t	blocks;
			dev_t		dev;
			dev_t		rdev;
		};

	protected:
		RamFileSystem& m_fs;
		FullInodeInfo m_inode_info;

		BAN::Vector<uint8_t> m_data;

		friend class RamFileSystem;
	};

	class RamDirectoryInode final : public RamInode
	{
	public:
		static BAN::ErrorOr<BAN::RefPtr<RamDirectoryInode>> create(RamFileSystem&, ino_t parent, mode_t, uid_t, gid_t);
		~RamDirectoryInode() = default;

		virtual BAN::ErrorOr<BAN::RefPtr<Inode>> directory_find_inode(BAN::StringView) override;
		virtual BAN::ErrorOr<void> directory_read_next_entries(off_t, DirectoryEntryList*, size_t) override;
		virtual BAN::ErrorOr<void> create_file(BAN::StringView, mode_t, uid_t, gid_t) override;
		
		BAN::ErrorOr<void> add_inode(BAN::StringView, BAN::RefPtr<RamInode>);

	private:
		RamDirectoryInode(RamFileSystem&, ino_t parent, mode_t, uid_t, gid_t);
	
	private:
		static constexpr size_t m_name_max = NAME_MAX;
		struct Entry
		{
			char name[m_name_max + 1];
			size_t name_len = 0;
			ino_t ino;
		};

	private:
		BAN::Vector<Entry> m_entries;
		ino_t m_parent;

		friend class RamFileSystem;
	};

	class RamSymlinkInode final : public RamInode
	{
	public:
		static BAN::ErrorOr<BAN::RefPtr<RamSymlinkInode>> create(RamFileSystem&, BAN::StringView target, mode_t, uid_t, gid_t);
		~RamSymlinkInode() = default;

		virtual off_t size() const override { return m_target.size(); }

		virtual BAN::ErrorOr<BAN::String> link_target() override;
		BAN::ErrorOr<void> set_link_target(BAN::StringView);

	private:
		RamSymlinkInode(RamFileSystem&, mode_t, uid_t, gid_t);

	private:
		BAN::String m_target;
	
		friend class RamFileSystem;
	};

}