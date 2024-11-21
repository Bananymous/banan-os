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
		void handle_input_data(size_t byte_count, uint8_t endpoint_id) override;

		BAN::ErrorOr<size_t> send_bytes(paddr_t, size_t count);
		BAN::ErrorOr<size_t> recv_bytes(paddr_t, size_t count);

		void lock()   { m_mutex.lock(); }
		void unlock() { m_mutex.unlock(); }

	private:
		USBMassStorageDriver(USBDevice&, const USBDevice::InterfaceDescriptor&);
		~USBMassStorageDriver();

		BAN::ErrorOr<void> initialize() override;

	private:
		USBDevice& m_device;
		USBDevice::InterfaceDescriptor m_interface;

		Mutex m_mutex;

		uint8_t m_in_endpoint_id { 0 };
		BAN::Function<void(size_t)> m_in_callback;

		uint8_t m_out_endpoint_id { 0 };
		BAN::Function<void(size_t)> m_out_callback;

		BAN::Vector<BAN::RefPtr<StorageDevice>> m_storage_devices;

		friend class BAN::UniqPtr<USBMassStorageDriver>;
	};

}
