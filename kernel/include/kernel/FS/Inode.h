#pragma once

#include <BAN/Memory.h>
#include <BAN/String.h>
#include <BAN/Vector.h>

namespace Kernel
{

	class Inode : public BAN::RefCounted<Inode>
	{
	public:
		union Mode
		{
			struct
			{
				uint16_t IXOTH : 1; // 0x0001
				uint16_t IWOTH : 1; // 0x0002
				uint16_t IROTH : 1; // 0x0004
				uint16_t IXGRP : 1; // 0x0008
				uint16_t IWGRP : 1; // 0x0010
				uint16_t IRGRP : 1; // 0x0020
				uint16_t IXUSR : 1; // 0x0040
				uint16_t IWUSR : 1; // 0x0080
				uint16_t IRUSR : 1; // 0x0100
				uint16_t ISVTX : 1; // 0x0200
				uint16_t ISGID : 1; // 0x0400
				uint16_t ISUID : 1; // 0x0800
				uint16_t IFIFO : 1; // 0x1000
				uint16_t IFCHR : 1; // 0x2000
				uint16_t IFDIR : 1; // 0x4000
				uint16_t IFREG : 1; // 0x8000
			};
			uint16_t mode;
		};

		enum class Type
		{
			Ext2,
		};

	public:
		virtual ~Inode() {}

		bool ifdir() const { return mode().IFDIR; }
		bool ifreg() const { return mode().IFREG; }

		virtual uint16_t uid() const = 0;
		virtual uint16_t gid() const = 0;
		virtual uint32_t size() const = 0;

		virtual Mode mode() const = 0;

		virtual BAN::StringView name() const = 0;

		BAN::ErrorOr<BAN::Vector<BAN::RefPtr<Inode>>> directory_inodes();
		BAN::ErrorOr<BAN::RefPtr<Inode>> directory_find(BAN::StringView);

		virtual BAN::ErrorOr<size_t> read(size_t, void*, size_t) = 0;

		virtual Type type() const = 0;
		virtual bool operator==(const Inode&) const = 0;

	protected:
		virtual BAN::ErrorOr<BAN::Vector<BAN::RefPtr<Inode>>> directory_inodes_impl() = 0;
		virtual BAN::ErrorOr<BAN::RefPtr<Inode>> directory_find_impl(BAN::StringView) = 0;
	};

}