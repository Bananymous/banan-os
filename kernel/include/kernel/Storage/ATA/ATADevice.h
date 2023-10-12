#pragma once

#include <kernel/Storage/ATA/ATABus.h>
#include <kernel/Storage/StorageDevice.h>

namespace Kernel
{

	namespace detail
	{

		class ATABaseDevice : public StorageDevice
		{
		public:
			enum class Command
			{
				Read,
				Write
			};

		public:
			virtual ~ATABaseDevice() {};

			virtual uint32_t sector_size() const override { return m_sector_words * 2; }
			virtual uint64_t total_size() const override { return m_lba_count * sector_size(); }

			uint32_t words_per_sector() const { return m_sector_words; }
			uint64_t sector_count() const { return m_lba_count; }

			BAN::StringView model() const { return m_model; }
			BAN::StringView name() const;

			virtual dev_t rdev() const override { return m_rdev; }

			virtual BAN::ErrorOr<size_t> read_impl(off_t, void*, size_t) override;
			virtual BAN::ErrorOr<size_t> write_impl(off_t, const void*, size_t) override;

		protected:
			ATABaseDevice();
			BAN::ErrorOr<void> initialize(BAN::Span<const uint16_t> identify_data);

		protected:			
			uint16_t m_signature;
			uint16_t m_capabilities;
			uint32_t m_command_set;
			uint32_t m_sector_words;
			uint64_t m_lba_count;
			char m_model[41];

			const dev_t m_rdev;
		};

	}

	class ATADevice final : public detail::ATABaseDevice
	{
	public:
		static BAN::ErrorOr<BAN::RefPtr<ATADevice>> create(BAN::RefPtr<ATABus>, ATABus::DeviceType, bool is_secondary, BAN::Span<const uint16_t> identify_data);

		bool is_secondary() const { return m_is_secondary; }

	private:
		ATADevice(BAN::RefPtr<ATABus>, ATABus::DeviceType, bool is_secodary);
		
		virtual BAN::ErrorOr<void> read_sectors_impl(uint64_t, uint64_t, uint8_t*) override;
		virtual BAN::ErrorOr<void> write_sectors_impl(uint64_t, uint64_t, const uint8_t*) override;

	private:
		BAN::RefPtr<ATABus> m_bus;
		const ATABus::DeviceType m_type;
		const bool m_is_secondary;
	};

}
