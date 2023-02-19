#pragma once

#include <BAN/ForwardList.h>
#include <BAN/Memory.h>

#include <stdint.h>

namespace Kernel
{

	class Inode
	{
	public:
		virtual bool is_directory() const = 0;
		virtual bool is_regular_file() const = 0;

		virtual uint16_t uid() const = 0;
		virtual uint16_t gid() const = 0;
		virtual uint32_t size() const = 0;

		virtual BAN::StringView name() const = 0;

		virtual BAN::ErrorOr<BAN::Vector<uint8_t>> read_all() const = 0;
		virtual BAN::ErrorOr<BAN::Vector<BAN::RefCounted<Inode>>> directory_inodes() const = 0;
		virtual BAN::ErrorOr<BAN::RefCounted<Inode>> directory_find(BAN::StringView) const = 0;
	};

}