#pragma once

#include <BAN/GUID.h>
#include <kernel/Device/Device.h>

namespace Kernel
{

	class Partition final : public BlockDevice
	{
	public:
		static BAN::ErrorOr<BAN::RefPtr<Partition>> create(BAN::RefPtr<BlockDevice>, const BAN::GUID& type, const BAN::GUID& guid, uint64_t first_block, uint64_t last_block, uint64_t attr, const char* label, uint32_t index, BAN::StringView name_prefix);

		const BAN::GUID& partition_type() const { return m_type; }
		const BAN::GUID& partition_guid() const { return m_guid; }

		virtual blksize_t blksize() const override { return m_block_size; }

		BAN::ErrorOr<void> read_sectors(uint64_t first_block, size_t block_count, BAN::ByteSpan buffer)			{ return read_blocks(first_block, block_count, buffer); }
		BAN::ErrorOr<void> write_sectors(uint64_t first_block, size_t block_count, BAN::ConstByteSpan buffer)	{ return write_blocks(first_block, block_count, buffer); }

		virtual BAN::ErrorOr<void> read_blocks(uint64_t first_block, size_t block_count, BAN::ByteSpan) override;
		virtual BAN::ErrorOr<void> write_blocks(uint64_t first_block, size_t block_count, BAN::ConstByteSpan) override;
		virtual BAN::ErrorOr<void> sync_blocks(uint64_t block, size_t block_count) override;

		virtual BAN::StringView name() const override { return m_name; }

		BAN::StringView uuid() const { return m_guid_string; }

	private:
		Partition(BAN::RefPtr<BlockDevice>, const BAN::GUID&, const BAN::GUID&, uint64_t, uint64_t, uint64_t, const char*, uint32_t, BAN::StringView);

	private:
		mutable BAN::WeakPtr<BlockDevice> m_device;
		const uint64_t m_block_size;
		const BAN::GUID m_type;
		const BAN::GUID m_guid;
		const BAN::String m_guid_string;
		const uint64_t m_first_block;
		const uint64_t m_last_block;
		const uint64_t m_attributes;
		char m_label[36 * 4 + 1];
		const BAN::String m_name;

	public:
		virtual bool is_partition() const override { return true; }

		virtual dev_t rdev() const override { return m_rdev; }

	protected:
		virtual BAN::ErrorOr<size_t> read_impl(off_t, BAN::ByteSpan) override;

		virtual bool can_read_impl() const override { return true; }
		virtual bool can_write_impl() const override { return true; }
		virtual bool has_error_impl() const override { return false; }

	private:
		const dev_t m_rdev;
	};

}
