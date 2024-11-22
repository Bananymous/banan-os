#pragma once

#include <BAN/Function.h>

#include <kernel/Lock/Mutex.h>
#include <kernel/Storage/StorageDevice.h>
#include <kernel/USB/Device.h>

namespace Kernel
{

	class USBMassStorageDriver final : public USBClassDriver
	{
		BAN_NON_COPYABLE(USBMassStorageDriver);
		BAN_NON_MOVABLE(USBMassStorageDriver);

	public:
		static constexpr size_t transfer_stall = -2;

	public:
		void handle_stall(uint8_t endpoint_id) override;
		void handle_input_data(size_t byte_count, uint8_t endpoint_id) override;

		BAN::ErrorOr<size_t> send_bytes(paddr_t, size_t count);
		BAN::ErrorOr<size_t> recv_bytes(paddr_t, size_t count);

		template<bool IN, typename SPAN>
		BAN::ErrorOr<size_t> send_command(uint8_t lun, BAN::ConstByteSpan scsi_command, SPAN data);

	private:
		USBMassStorageDriver(USBDevice&, const USBDevice::InterfaceDescriptor&);
		~USBMassStorageDriver();

		BAN::ErrorOr<void> initialize() override;
		BAN::ErrorOr<void> mass_storage_reset();
		BAN::ErrorOr<void> clear_feature(uint8_t endpoint_id);
		BAN::ErrorOr<void> reset_recovery();

	private:
		USBDevice& m_device;
		USBDevice::InterfaceDescriptor m_interface;

		Mutex m_mutex;

		uint8_t m_in_endpoint_id { 0 };
		BAN::Function<void(size_t)> m_in_callback;

		uint8_t m_out_endpoint_id { 0 };
		BAN::Function<void(size_t)> m_out_callback;

		BAN::UniqPtr<DMARegion> m_data_region;

		BAN::Vector<BAN::RefPtr<StorageDevice>> m_storage_devices;

		friend class BAN::UniqPtr<USBMassStorageDriver>;
	};

}
