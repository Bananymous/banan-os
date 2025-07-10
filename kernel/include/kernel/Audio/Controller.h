#pragma once

#include <kernel/Device/Device.h>
#include <kernel/PCI.h>

namespace Kernel
{

	class AudioController : public CharacterDevice
	{
	public:
		static BAN::ErrorOr<BAN::RefPtr<AudioController>> create(PCI::Device& pci_device);

		dev_t rdev() const override { return m_rdev; }
		BAN::StringView name() const override { return m_name; }

	protected:
		AudioController();

		virtual void handle_new_data() = 0;

		virtual uint32_t get_channels() const = 0;
		virtual uint32_t get_sample_rate() const = 0;

		bool can_read_impl() const override { return false; }
		bool can_write_impl() const override { SpinLockGuard _(m_spinlock); return m_sample_data_size < m_sample_data_capacity; }
		bool has_error_impl() const override { return false; }
		bool has_hungup_impl() const override { return false; }

		BAN::ErrorOr<size_t> write_impl(off_t, BAN::ConstByteSpan) override;

		BAN::ErrorOr<long> ioctl_impl(int cmd, void* arg) override;

	protected:
		ThreadBlocker m_sample_data_blocker;
		mutable SpinLock m_spinlock;

		static constexpr size_t m_sample_data_capacity = 1 << 20;
		uint8_t m_sample_data[m_sample_data_capacity];
		size_t m_sample_data_head { 0 };
		size_t m_sample_data_size { 0 };

	private:
		const dev_t m_rdev;
		char m_name[10] {};
	};

}
