#include <BAN/Endianness.h>
#include <BAN/ScopeGuard.h>
#include <BAN/StringView.h>

#include <kernel/FS/VirtualFileSystem.h>
#include <kernel/Lock/LockGuard.h>
#include <kernel/Timer/Timer.h>
#include <kernel/USB/MassStorage/MassStorageDriver.h>
#include <kernel/USB/MassStorage/SCSIDevice.h>

namespace Kernel
{

	USBMassStorageDriver::USBMassStorageDriver(USBDevice& device, const USBDevice::InterfaceDescriptor& interface)
		: m_device(device)
		, m_interface(interface)
	{ }

	USBMassStorageDriver::~USBMassStorageDriver()
	{ }

	BAN::ErrorOr<void> USBMassStorageDriver::initialize()
	{
		if (m_interface.descriptor.bInterfaceProtocol != 0x50)
		{
			dwarnln("Only USB Mass Storage BBB is supported");
			return BAN::Error::from_errno(ENOTSUP);
		}

		auto dma_region = TRY(DMARegion::create(PAGE_SIZE));

		// Bulk-Only Mass Storage Reset
		{
			USBDeviceRequest reset_request {
				.bmRequestType = USB::RequestType::HostToDevice | USB::RequestType::Class | USB::RequestType::Interface,
				.bRequest      = 0xFF,
				.wValue        = 0x0000,
				.wIndex        = m_interface.descriptor.bInterfaceNumber,
				.wLength       = 0x0000,
			};

			TRY(m_device.send_request(reset_request, 0));
		}

		// Get Max LUN
		{
			USBDeviceRequest lun_request {
				.bmRequestType = USB::RequestType::DeviceToHost | USB::RequestType::Class | USB::RequestType::Interface,
				.bRequest      = 0xFE,
				.wValue        = 0x0000,
				.wIndex        = m_interface.descriptor.bInterfaceNumber,
				.wLength       = 0x0001,
			};

			uint32_t max_lun = 0;
			const auto lun_result = m_device.send_request(lun_request, dma_region->paddr());
			if (!lun_result.is_error() && lun_result.value() == 1)
				max_lun = *reinterpret_cast<uint8_t*>(dma_region->vaddr());
			TRY(m_storage_devices.resize(max_lun + 1));
		}

		uint32_t max_packet_size = -1;

		// Initialize bulk-in and bulk-out endpoints
		{
			constexpr size_t invalid_index = -1;

			size_t bulk_in_index = invalid_index;
			size_t bulk_out_index = invalid_index;

			for (size_t i = 0; i < m_interface.endpoints.size(); i++)
			{
				const auto& endpoint = m_interface.endpoints[i].descriptor;
				if (endpoint.bmAttributes != 0x02)
					continue;
				((endpoint.bEndpointAddress & 0x80) ? bulk_in_index : bulk_out_index) = i;
			}

			if (bulk_in_index == invalid_index || bulk_out_index == invalid_index)
			{
				dwarnln("USB Mass Storage device does not contain bulk-in and bulk-out endpoints");
				return BAN::Error::from_errno(EFAULT);
			}

			TRY(m_device.initialize_endpoint(m_interface.endpoints[bulk_in_index].descriptor));
			TRY(m_device.initialize_endpoint(m_interface.endpoints[bulk_out_index].descriptor));

			{
				const auto& desc = m_interface.endpoints[bulk_in_index].descriptor;
				m_in_endpoint_id = (desc.bEndpointAddress & 0x0F) * 2 + !!(desc.bEndpointAddress & 0x80);
				max_packet_size = BAN::Math::min<uint32_t>(max_packet_size, desc.wMaxPacketSize);
			}

			{
				const auto& desc = m_interface.endpoints[bulk_out_index].descriptor;
				m_out_endpoint_id = (desc.bEndpointAddress & 0x0F) * 2 + !!(desc.bEndpointAddress & 0x80);
				max_packet_size = BAN::Math::min<uint32_t>(max_packet_size, desc.wMaxPacketSize);
			}
		}

		BAN::Function<BAN::ErrorOr<BAN::RefPtr<StorageDevice>>(USBMassStorageDriver&, uint8_t, uint32_t)> create_device_func;
		switch (m_interface.descriptor.bInterfaceSubClass)
		{
			case 0x06:
				create_device_func =
					[](USBMassStorageDriver& driver, uint8_t lun, uint32_t max_packet_size) -> BAN::ErrorOr<BAN::RefPtr<StorageDevice>>
					{
						auto ret = TRY(USBSCSIDevice::create(driver, lun, max_packet_size));
						return BAN::RefPtr<StorageDevice>(ret);
					};
				break;
			default:
				dwarnln("Unsupported command block {2H}", m_interface.descriptor.bInterfaceSubClass);
				return BAN::Error::from_errno(ENOTSUP);
		}

		ASSERT(m_storage_devices.size() <= 0xFF);
		for (uint8_t lun = 0; lun < m_storage_devices.size(); lun++)
			m_storage_devices[lun] = TRY(create_device_func(*this, lun, max_packet_size));

		return {};
	}

	BAN::ErrorOr<size_t> USBMassStorageDriver::send_bytes(paddr_t paddr, size_t count)
	{
		ASSERT(m_mutex.is_locked());

		constexpr size_t invalid = -1;

		static volatile size_t bytes_sent;
		bytes_sent = invalid;

		ASSERT(!m_out_callback);
		m_out_callback = [](size_t bytes) { bytes_sent = bytes; };
		BAN::ScopeGuard _([this] { m_out_callback.clear(); });

		m_device.send_data_buffer(m_out_endpoint_id, paddr, count);

		const uint64_t timeout_ms = SystemTimer::get().ms_since_boot() + 100;
		while (bytes_sent == invalid)
			if (SystemTimer::get().ms_since_boot() > timeout_ms)
				return BAN::Error::from_errno(EIO);

		return static_cast<size_t>(bytes_sent);
	}

	BAN::ErrorOr<size_t> USBMassStorageDriver::recv_bytes(paddr_t paddr, size_t count)
	{
		ASSERT(m_mutex.is_locked());

		constexpr size_t invalid = -1;

		static volatile size_t bytes_recv;
		bytes_recv = invalid;

		ASSERT(!m_in_callback);
		m_in_callback = [](size_t bytes) { bytes_recv = bytes; };
		BAN::ScopeGuard _([this] { m_in_callback.clear(); });

		m_device.send_data_buffer(m_in_endpoint_id, paddr, count);

		const uint64_t timeout_ms = SystemTimer::get().ms_since_boot() + 100;
		while (bytes_recv == invalid)
			if (SystemTimer::get().ms_since_boot() > timeout_ms)
				return BAN::Error::from_errno(EIO);

		m_in_callback.clear();

		return static_cast<size_t>(bytes_recv);
	}

	void USBMassStorageDriver::handle_input_data(size_t byte_count, uint8_t endpoint_id)
	{
		if (endpoint_id != m_in_endpoint_id && endpoint_id != m_out_endpoint_id)
			return;

		dprintln_if(DEBUG_USB_MASS_STORAGE, "got {} bytes to {} endpoint", byte_count, endpoint_id == m_in_endpoint_id ? "IN" : "OUT");

		if (endpoint_id == m_in_endpoint_id)
		{
			if (m_in_callback)
				m_in_callback(byte_count);
			else
				dwarnln("ignoring {} bytes to IN endpoint", byte_count);
			return;
		}

		if (endpoint_id == m_out_endpoint_id)
		{
			if (m_out_callback)
				m_out_callback(byte_count);
			else
				dwarnln("ignoring {} bytes to OUT endpoint", byte_count);
			return;
		}

	}

}
