#pragma once

#include <BAN/Vector.h>
#include <kernel/InterruptController.h>
#include <kernel/PCI.h>
#include <kernel/Storage/NVMe/Definitions.h>
#include <kernel/Storage/NVMe/Namespace.h>
#include <kernel/Storage/NVMe/Queue.h>

namespace Kernel
{

	class NVMeController final : public StorageController, public CharacterDevice
	{
		BAN_NON_COPYABLE(NVMeController);
		BAN_NON_MOVABLE(NVMeController);

	public:
		static BAN::ErrorOr<BAN::RefPtr<StorageController>> create(PCI::Device&);

		NVMeQueue& io_queue() { return *m_io_queue; }

		virtual dev_t rdev() const override { return m_rdev; }
		virtual BAN::StringView name() const override { return m_name; }

	protected:
		virtual bool can_read_impl() const override { return false; }
		virtual bool can_write_impl() const override { return false; }
		virtual bool has_error_impl() const override { return false; }
		virtual bool has_hungup_impl() const override { return false; }

	private:
		NVMeController(PCI::Device& pci_device);
		virtual BAN::ErrorOr<void> initialize() override;

		BAN::ErrorOr<void> identify_controller();
		BAN::ErrorOr<void> identify_namespaces();

		BAN::ErrorOr<void> wait_until_ready(bool expected_value);
		BAN::ErrorOr<void> create_admin_queue();
		BAN::ErrorOr<void> create_io_queue();

	private:
		PCI::Device& m_pci_device;
		BAN::UniqPtr<PCI::BarRegion> m_bar0;
		volatile NVMe::ControllerRegisters* m_controller_registers;

		BAN::UniqPtr<NVMeQueue> m_admin_queue;
		BAN::UniqPtr<NVMeQueue> m_io_queue;

		BAN::Vector<BAN::RefPtr<NVMeNamespace>> m_namespaces;

		char m_name[20];
		const dev_t m_rdev;
	};

}
