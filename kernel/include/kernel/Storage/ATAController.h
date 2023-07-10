#pragma once

#include <kernel/PCI.h>
#include <kernel/SpinLock.h>
#include <kernel/Storage/StorageController.h>

namespace Kernel
{

	class ATABus;

	class ATAController final : public StorageController
	{
	public:
		static BAN::ErrorOr<BAN::RefPtr<ATAController>> create(const PCIDevice&);

		virtual BAN::Vector<BAN::RefPtr<StorageDevice>> devices() override;

	private:
		ATAController();
		BAN::ErrorOr<void> initialize(const PCIDevice& device);

	private:
		ATABus* m_buses[2] { nullptr, nullptr };
		friend class ATABus;

	public:
		virtual Mode mode() const override { return { Mode::IFCHR }; }
		virtual uid_t uid() const override { return 0; }
		virtual gid_t gid() const override { return 0; }
		virtual dev_t rdev() const override { return m_rdev; }

		virtual BAN::ErrorOr<size_t> read(size_t, void*, size_t) { return BAN::Error::from_errno(ENOTSUP); }
	
	private:
		const dev_t m_rdev;
	};

}