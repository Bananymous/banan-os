#include <kernel/FS/DevFS/FileSystem.h>
#include <kernel/Storage/NVMe/Controller.h>
#include <kernel/Storage/NVMe/Namespace.h>

#include <sys/sysmacros.h>

namespace Kernel
{

	static dev_t get_ns_dev_major()
	{
		static dev_t major = DevFileSystem::get().get_next_dev();
		return major;
	}

	static dev_t get_ns_dev_minor()
	{
		static dev_t minor = 0;
		return minor++;
	}

	BAN::ErrorOr<BAN::RefPtr<NVMeNamespace>> NVMeNamespace::create(NVMeController& controller, uint32_t nsid, uint64_t block_count, uint32_t block_size)
	{
		auto* namespace_ptr = new NVMeNamespace(controller, nsid, block_count, block_size);
		if (namespace_ptr == nullptr)
			return BAN::Error::from_errno(ENOMEM);
		auto ns = BAN::RefPtr<NVMeNamespace>::adopt(namespace_ptr);
		TRY(ns->initialize());
		return ns;
	}

	NVMeNamespace::NVMeNamespace(NVMeController& controller, uint32_t nsid, uint64_t block_count, uint32_t block_size)
		: m_controller(controller)
		, m_nsid(nsid)
		, m_block_size(block_size)
		, m_block_count(block_count)
		, m_rdev(makedev(get_ns_dev_major(), get_ns_dev_minor()))
	{
		ASSERT(minor(m_rdev) < 10);
		ASSERT(m_controller.name().size() + 2 < sizeof(m_name));
		memcpy(m_name, m_controller.name().data(), m_controller.name().size());
		m_name[m_controller.name().size() + 0] = 'n';
		m_name[m_controller.name().size() + 1] = '1' + minor(m_rdev);
		m_name[m_controller.name().size() + 2] = '\0';
	}

	BAN::ErrorOr<void> NVMeNamespace::initialize()
	{
		m_dma_region = TRY(DMARegion::create(PAGE_SIZE));

		add_disk_cache();

		DevFileSystem::get().add_device(this);

		char name_prefix[20];
		strcpy(name_prefix, m_name);
		strcat(name_prefix, "p");
		if (auto res = initialize_partitions(name_prefix); res.is_error())
			dprintln("{}", res.error());

		return {};
	}

	BAN::ErrorOr<void> NVMeNamespace::read_sectors_impl(uint64_t lba, uint64_t sector_count, BAN::ByteSpan buffer)
	{
		ASSERT(buffer.size() >= sector_count * m_block_size);

		for (uint64_t i = 0; i < sector_count;)
		{
			uint16_t count = BAN::Math::min(sector_count - i, m_dma_region->size() / m_block_size);

			NVMe::SubmissionQueueEntry sqe {};
			sqe.opc = NVMe::OPC_IO_READ;
			sqe.read.nsid = m_nsid;
			sqe.read.dptr.prp1 = m_dma_region->paddr();
			sqe.read.slba = lba + i;
			sqe.read.nlb = count - 1;
			if (uint16_t status = m_controller.io_queue().submit_command(sqe))
			{
				dwarnln("NVMe read failed (status {4H})", status);
				return BAN::Error::from_errno(EIO);
			}
			memcpy(buffer.data() + i * m_block_size, reinterpret_cast<void*>(m_dma_region->vaddr()), count * m_block_size);

			i += count;
		}

		return {};
	}

	BAN::ErrorOr<void> NVMeNamespace::write_sectors_impl(uint64_t lba, uint64_t sector_count, BAN::ConstByteSpan buffer)
	{
		ASSERT(buffer.size() >= sector_count * m_block_size);

		for (uint64_t i = 0; i < sector_count;)
		{
			uint16_t count = BAN::Math::min(sector_count - i, m_dma_region->size() / m_block_size);

			memcpy(reinterpret_cast<void*>(m_dma_region->vaddr()), buffer.data() + i * m_block_size, count * m_block_size);

			NVMe::SubmissionQueueEntry sqe {};
			sqe.opc = NVMe::OPC_IO_WRITE;
			sqe.read.nsid = m_nsid;
			sqe.read.dptr.prp1 = m_dma_region->paddr();
			sqe.read.slba = lba + i;
			sqe.read.nlb = count - 1;
			if (uint16_t status = m_controller.io_queue().submit_command(sqe))
			{
				dwarnln("NVMe write failed (status {4H})", status);
				return BAN::Error::from_errno(EIO);
			}

			i += count;
		}

		return {};
	}

}
