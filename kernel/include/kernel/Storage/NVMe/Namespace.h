#pragma once

#include <kernel/Memory/DMARegion.h>
#include <kernel/Storage/StorageDevice.h>

namespace Kernel
{

	class NVMeController;

	class NVMeNamespace : public StorageDevice
	{
	public:
		static BAN::ErrorOr<BAN::RefPtr<NVMeNamespace>> create(NVMeController&, uint32_t ns_index, uint32_t nsid, uint64_t block_count, uint32_t block_size);

		virtual uint32_t sector_size() const override { return m_block_size; }
		virtual uint64_t total_size() const override { return m_block_size * m_block_count; }

		virtual dev_t rdev() const override { return m_rdev; }
		virtual BAN::StringView name() const { return m_name; }

	private:
		NVMeNamespace(NVMeController&, uint32_t ns_index, uint32_t nsid, uint64_t block_count, uint32_t block_size);
		BAN::ErrorOr<void> initialize();

		virtual BAN::ErrorOr<void> read_sectors_impl(uint64_t lba, uint64_t sector_count, BAN::ByteSpan) override;
		virtual BAN::ErrorOr<void> write_sectors_impl(uint64_t lba, uint64_t sector_count, BAN::ConstByteSpan) override;

	private:
		NVMeController& m_controller;
		BAN::UniqPtr<DMARegion> m_dma_region;

		const uint32_t m_nsid;
		const uint32_t m_block_size;
		const uint64_t m_block_count;

		char m_name[10] {};
		const dev_t m_rdev;
	};

}
