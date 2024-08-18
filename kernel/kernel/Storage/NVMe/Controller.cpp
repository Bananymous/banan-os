#include <BAN/Array.h>
#include <kernel/Device/DeviceNumbers.h>
#include <kernel/FS/DevFS/FileSystem.h>
#include <kernel/Memory/DMARegion.h>
#include <kernel/Storage/NVMe/Controller.h>
#include <kernel/Timer/Timer.h>

#include <sys/sysmacros.h>

#define DEBUG_NVMe 1

namespace Kernel
{

	static dev_t get_ctrl_dev_minor()
	{
		static dev_t minor = 0;
		return minor++;
	}

	BAN::ErrorOr<BAN::RefPtr<StorageController>> NVMeController::create(PCI::Device& pci_device)
	{
		auto* controller_ptr = new NVMeController(pci_device);
		if (controller_ptr == nullptr)
			return BAN::Error::from_errno(ENOMEM);
		auto controller = BAN::RefPtr<StorageController>::adopt(controller_ptr);
		TRY(controller->initialize());
		return controller;
	}

	NVMeController::NVMeController(PCI::Device& pci_device)
		: CharacterDevice(0600, 0, 0)
		, m_pci_device(pci_device)
		, m_rdev(makedev(DeviceNumber::NVMeController, get_ctrl_dev_minor()))
	{
		ASSERT(minor(m_rdev) < 10);
		strcpy(m_name, "nvmeX");
		m_name[4] = '0' + minor(m_rdev);
	}

	BAN::ErrorOr<void> NVMeController::initialize()
	{
		// See NVM express base specification section 3.5.1
		m_pci_device.enable_bus_mastering();
		m_pci_device.enable_memory_space();

		m_bar0 = TRY(m_pci_device.allocate_bar_region(0));
		if (m_bar0->type() != PCI::BarType::MEM)
		{
			dwarnln("NVMe controller BAR0 is not MEM");
			return BAN::Error::from_errno(EINVAL);
		}
		if (m_bar0->size() < 0x1000)
		{
			dwarnln("NVMe controller BAR0 is too small {} bytes", m_bar0->size());
			return BAN::Error::from_errno(EINVAL);
		}

		m_controller_registers = reinterpret_cast<volatile NVMe::ControllerRegisters*>(m_bar0->vaddr());

		const auto& vs = m_controller_registers->vs;
		if (vs.major != 1)
		{
			dwarnln("NVMe controller has unsupported version {}.{}", (uint16_t)vs.major, (uint8_t)vs.minor);
			return BAN::Error::from_errno(ENOTSUP);
		}

		dprintln_if(DEBUG_NVMe, "NVMe controller");
		dprintln_if(DEBUG_NVMe, "  version: {}.{}", (uint16_t)vs.major, (uint8_t)vs.minor);

		auto& cap = m_controller_registers->cap;
		if (!(cap.css & NVMe::CAP_CSS_NVME))
		{
			dwarnln("NVMe controller does not support NVMe command set");
			return BAN::Error::from_errno(ECANCELED);
		}

		const uint64_t min_page_size = 1ull << (12 + cap.mpsmin);
		const uint64_t max_page_size = 1ull << (12 + cap.mpsmax);
		if (PAGE_SIZE < min_page_size || PAGE_SIZE > max_page_size)
		{
			dwarnln("NVMe controller does not support {} byte pages, only {}-{} byte pages are supported", PAGE_SIZE, min_page_size, max_page_size);
			return BAN::Error::from_errno(ECANCELED);
		}

		// One for aq and one for ioq
		TRY(m_pci_device.reserve_irqs(2));

		auto& cc = m_controller_registers->cc;

		if (cc.en)
			TRY(wait_until_ready(true));
		cc.en = 0;
		TRY(wait_until_ready(false));
		dprintln_if(DEBUG_NVMe, "  controller reset");

		TRY(create_admin_queue());
		dprintln_if(DEBUG_NVMe, "  created admin queue");

		// Configure controller
		cc.ams = 0;
		cc.mps = PAGE_SIZE_SHIFT - 12;
		cc.css = 0b000;

		cc.en = 1;
		TRY(wait_until_ready(true));
		dprintln_if(DEBUG_NVMe, " controller enabled");

		TRY(identify_controller());

		cc.iocqes = 4; static_assert(1 << 4 == sizeof(NVMe::CompletionQueueEntry));
		cc.iosqes = 6; static_assert(1 << 6 == sizeof(NVMe::SubmissionQueueEntry));
		TRY(create_io_queue());
		dprintln_if(DEBUG_NVMe, " created io queue");

		TRY(identify_namespaces());

		DevFileSystem::get().add_device(this);

		StorageController::ref();

		return {};
	}

	BAN::ErrorOr<void> NVMeController::wait_until_ready(bool expected_value)
	{
		const auto& cap = m_controller_registers->cap;
		const auto& csts = m_controller_registers->csts;

		uint64_t timeout = SystemTimer::get().ms_since_boot() + 500 * cap.to;
		while (csts.rdy != expected_value)
		{
			if (SystemTimer::get().ms_since_boot() >= timeout)
			{
				dwarnln("NVMe controller reset timedout");
				return BAN::Error::from_errno(ETIMEDOUT);
			}
		}

		return {};
	}

	BAN::ErrorOr<void> NVMeController::identify_controller()
	{
		auto dma_page = TRY(DMARegion::create(PAGE_SIZE));

		NVMe::SubmissionQueueEntry sqe {};
		sqe.opc = NVMe::OPC_ADMIN_IDENTIFY;
		sqe.identify.dptr.prp1 = dma_page->paddr();
		sqe.identify.cns = NVMe::CNS_INDENTIFY_CONTROLLER;
		if (uint16_t status = m_admin_queue->submit_command(sqe))
		{
			dwarnln("NVMe controller identify failed (status {4H})", status);
			return BAN::Error::from_errno(EFAULT);
		}

		if (*reinterpret_cast<uint16_t*>(dma_page->vaddr()) != m_pci_device.vendor_id())
		{
			dwarnln("NVMe controller vendor id does not match with the one in PCI");
			return BAN::Error::from_errno(EFAULT);
		}

		dprintln_if(DEBUG_NVMe, " model: '{}'", BAN::StringView { (char*)dma_page->vaddr() + 24, 20 });

		return {};
	}

	BAN::ErrorOr<void> NVMeController::identify_namespaces()
	{
		auto dma_page = TRY(DMARegion::create(PAGE_SIZE));

		BAN::Vector<uint32_t> namespace_ids;
		TRY(namespace_ids.resize(PAGE_SIZE / sizeof(uint32_t)));

		{
			NVMe::SubmissionQueueEntry sqe {};
			sqe.opc = NVMe::OPC_ADMIN_IDENTIFY;
			sqe.identify.dptr.prp1 = dma_page->paddr();
			sqe.identify.cns = NVMe::CNS_INDENTIFY_ACTIVE_NAMESPACES;
			if (uint16_t status = m_admin_queue->submit_command(sqe))
			{
				dwarnln("NVMe active namespace identify failed (status {4H})", status);
				return BAN::Error::from_errno(EFAULT);
			}
			memcpy(namespace_ids.data(), reinterpret_cast<void*>(dma_page->vaddr()), PAGE_SIZE);
		}

		for (uint32_t nsid : namespace_ids)
		{
			if (nsid == 0)
				break;
			dprintln(" found namespace {}", nsid);

			NVMe::SubmissionQueueEntry sqe {};
			sqe.opc = NVMe::OPC_ADMIN_IDENTIFY;
			sqe.identify.nsid = nsid;
			sqe.identify.dptr.prp1 = dma_page->paddr();
			sqe.identify.cns = NVMe::CNS_INDENTIFY_NAMESPACE;
			if (uint16_t status = m_admin_queue->submit_command(sqe))
			{
				dwarnln("NVMe namespace {} identify failed (status {4H})", nsid , status);
				return BAN::Error::from_errno(EFAULT);
			}

			auto& namespace_info = *reinterpret_cast<volatile NVMe::NamespaceIdentify*>(dma_page->vaddr());

			const uint64_t block_count = namespace_info.nsze;

			const uint64_t format = namespace_info.lbafN[namespace_info.flbas & 0x0F];
			const uint64_t block_size = 1u << ((format >> 16) & 0xFF);

			dprintln("   block count {}",     block_count);
			dprintln("   block size  {} B",   block_size);
			dprintln("   total       {} MiB", block_count * block_size / (1 << 20));

			auto ns = TRY(NVMeNamespace::create(*this, m_namespaces.size(), nsid, block_count, block_size));
			TRY(m_namespaces.push_back(BAN::move(ns)));
		}

		return {};
	}

	BAN::ErrorOr<void> NVMeController::create_admin_queue()
	{
		const uint32_t admin_queue_depth = BAN::Math::min(PAGE_SIZE / sizeof(NVMe::CompletionQueueEntry), PAGE_SIZE / sizeof(NVMe::SubmissionQueueEntry));
		auto& aqa = m_controller_registers->aqa;
		aqa.acqs = admin_queue_depth - 1;
		aqa.asqs = admin_queue_depth - 1;
		dprintln_if(DEBUG_NVMe, " admin queue depth is {}", admin_queue_depth);

		const uint32_t completion_queue_size = admin_queue_depth * sizeof(NVMe::CompletionQueueEntry);
		auto completion_queue = TRY(DMARegion::create(completion_queue_size));
		memset((void*)completion_queue->vaddr(), 0x00, completion_queue->size());

		const uint32_t submission_queue_size = admin_queue_depth * sizeof(NVMe::SubmissionQueueEntry);
		auto submission_queue = TRY(DMARegion::create(submission_queue_size));
		memset((void*)submission_queue->vaddr(), 0x00, submission_queue->size());

		m_controller_registers->acq = completion_queue->paddr();
		m_controller_registers->asq = submission_queue->paddr();

		uint8_t irq = m_pci_device.get_irq(0);
		dprintln_if(DEBUG_NVMe, " admin queue using irq {}", irq);

		auto& doorbell = *reinterpret_cast<volatile NVMe::DoorbellRegisters*>(m_bar0->vaddr() + NVMe::ControllerRegisters::SQ0TDBL);

		m_admin_queue = TRY(BAN::UniqPtr<NVMeQueue>::create(BAN::move(completion_queue), BAN::move(submission_queue), doorbell, admin_queue_depth, irq));

		return {};
	}

	BAN::ErrorOr<void> NVMeController::create_io_queue()
	{
		constexpr uint32_t queue_size = PAGE_SIZE;
		constexpr uint32_t queue_elems = queue_size / BAN::Math::max(sizeof(NVMe::CompletionQueueEntry), sizeof(NVMe::SubmissionQueueEntry));
		auto completion_queue = TRY(DMARegion::create(queue_size));
		memset((void*)completion_queue->vaddr(), 0x00, completion_queue->size());

		auto submission_queue = TRY(DMARegion::create(queue_size));
		memset((void*)submission_queue->vaddr(), 0x00, submission_queue->size());

		{
			NVMe::SubmissionQueueEntry sqe {};
			sqe.opc = NVMe::OPC_ADMIN_CREATE_CQ;
			sqe.create_cq.dptr.prp1 = completion_queue->paddr();
			sqe.create_cq.qsize = queue_elems - 1;
			sqe.create_cq.qid = 1;
			sqe.create_cq.iv = 1;
			sqe.create_cq.ien = 1;
			sqe.create_cq.pc = 1;
			if (uint16_t status = m_admin_queue->submit_command(sqe))
			{
				dwarnln("NVMe io completion queue creation failed (status {4H})", status);
				return BAN::Error::from_errno(EFAULT);
			}
		}

		{
			NVMe::SubmissionQueueEntry sqe {};
			sqe.opc = NVMe::OPC_ADMIN_CREATE_SQ;
			sqe.create_sq.dptr.prp1 = submission_queue->paddr();
			sqe.create_sq.qsize = queue_elems - 1;
			sqe.create_sq.qid = 1;
			sqe.create_sq.cqid = 1;
			sqe.create_sq.qprio = 0;
			sqe.create_sq.pc = 1;
			sqe.create_sq.nvmsetid = 0;
			if (uint16_t status = m_admin_queue->submit_command(sqe))
			{
				dwarnln("NVMe io submission queue creation failed (status {4H})", status);
				return BAN::Error::from_errno(EFAULT);
			}
		}

		uint8_t irq = m_pci_device.get_irq(1);
		dprintln_if(DEBUG_NVMe, " io queue using irq {}", irq);

		const uint32_t doorbell_stride = 1 << (2 + m_controller_registers->cap.dstrd);
		const uint32_t doorbell_offset = 2 * doorbell_stride;
		auto& doorbell = *reinterpret_cast<volatile NVMe::DoorbellRegisters*>(m_bar0->vaddr() + NVMe::ControllerRegisters::SQ0TDBL + doorbell_offset);

		m_io_queue = TRY(BAN::UniqPtr<NVMeQueue>::create(BAN::move(completion_queue), BAN::move(submission_queue), doorbell, queue_elems, irq));

		return {};
	}

}
