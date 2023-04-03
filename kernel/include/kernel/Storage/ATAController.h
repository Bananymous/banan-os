#pragma once

#include <kernel/DeviceManager.h>
#include <kernel/PCI.h>
#include <kernel/SpinLock.h>
#include <kernel/Storage/StorageController.h>

namespace Kernel
{

	class ATABus;

	class ATAController final : public StorageController
	{
	public:
		static BAN::ErrorOr<ATAController*> create(const PCIDevice&);

		virtual BAN::Vector<StorageDevice*> devices() override;

		uint8_t next_device_index() const;

	private:
		ATAController()
			: m_rdev(makedev(DeviceManager::get().get_next_rdev(), 0))
		{ }
		BAN::ErrorOr<void> initialize(const PCIDevice& device);

	private:
		ATABus* m_buses[2] { nullptr, nullptr };
		friend class ATABus;

	public:
		virtual Mode mode() const override { return { Mode::IFCHR }; }
		virtual uid_t uid() const override { return 0; }
		virtual gid_t gid() const override { return 0; }
		virtual dev_t rdev() const override { return m_rdev; }

		virtual BAN::StringView name() const override { return "hd"sv; }

		virtual BAN::ErrorOr<size_t> read(size_t, void*, size_t) { return BAN::Error::from_errno(ENOTSUP); }
	
	private:
		const dev_t m_rdev;
	};

}