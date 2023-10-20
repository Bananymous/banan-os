#pragma once

#include <BAN/ByteSpan.h>
#include <BAN/RefPtr.h>
#include <BAN/Vector.h>
#include <kernel/InterruptController.h>
#include <kernel/SpinLock.h>

namespace Kernel
{

	class ATADevice;

	class ATABus : public BAN::RefCounted<ATABus>, public Interruptable
	{
	public:
		enum class DeviceType
		{
			ATA,
			ATAPI,
		};

	public:
		static BAN::ErrorOr<BAN::RefPtr<ATABus>> create(uint16_t base, uint16_t ctrl, uint8_t irq);

		BAN::ErrorOr<void> read(ATADevice&, uint64_t lba, uint64_t sector_count, BAN::ByteSpan);
		BAN::ErrorOr<void> write(ATADevice&, uint64_t lba, uint64_t sector_count, BAN::ConstByteSpan);

		virtual void handle_irq() override;

	private:
		ATABus(uint16_t base, uint16_t ctrl)
			: m_base(base)
			, m_ctrl(ctrl)
		{}
		BAN::ErrorOr<void> initialize();

		void select_device(bool secondary);
		BAN::ErrorOr<DeviceType> identify(bool secondary, BAN::Span<uint16_t> buffer);

		void block_until_irq();
		//uint8_t device_index(const ATADevice&) const;

		uint8_t io_read(uint16_t);
		void io_write(uint16_t, uint8_t);
		void read_buffer(uint16_t, uint16_t*, size_t);
		void write_buffer(uint16_t, const uint16_t*, size_t);
		BAN::ErrorOr<void> wait(bool);
		BAN::Error error();

	private:
		const uint16_t m_base;
		const uint16_t m_ctrl;
		SpinLock m_lock;

		volatile bool m_has_got_irq { false };

		// Non-owning pointers
		BAN::Vector<ATADevice*> m_devices;

		friend class ATAController;
	};

}