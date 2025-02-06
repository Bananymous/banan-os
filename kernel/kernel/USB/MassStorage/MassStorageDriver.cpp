#include <BAN/Endianness.h>
#include <BAN/ScopeGuard.h>
#include <BAN/StringView.h>

#include <kernel/FS/VirtualFileSystem.h>
#include <kernel/Lock/LockGuard.h>
#include <kernel/Timer/Timer.h>
#include <kernel/USB/MassStorage/Definitions.h>
#include <kernel/USB/MassStorage/MassStorageDriver.h>
#include <kernel/USB/MassStorage/SCSIDevice.h>

namespace Kernel
{

	static constexpr uint64_t s_timeout_ms = 10'000;

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

		m_data_region = TRY(DMARegion::create(PAGE_SIZE));

		TRY(mass_storage_reset());

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
			const auto lun_result = m_device.send_request(lun_request, m_data_region->paddr());
			if (!lun_result.is_error() && lun_result.value() == 1)
				max_lun = *reinterpret_cast<uint8_t*>(m_data_region->vaddr());
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

			TRY(m_device.configure_endpoint(m_interface.endpoints[bulk_in_index].descriptor));
			TRY(m_device.configure_endpoint(m_interface.endpoints[bulk_out_index].descriptor));

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
						return BAN::RefPtr<StorageDevice>(
							TRY(USBSCSIDevice::create(driver, lun, max_packet_size))
						);
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

	BAN::ErrorOr<void> USBMassStorageDriver::mass_storage_reset()
	{
		USBDeviceRequest reset_request {
			.bmRequestType = USB::RequestType::HostToDevice | USB::RequestType::Class | USB::RequestType::Interface,
			.bRequest      = 0xFF,
			.wValue        = 0x0000,
			.wIndex        = m_interface.descriptor.bInterfaceNumber,
			.wLength       = 0x0000,
		};

		TRY(m_device.send_request(reset_request, 0));
		return {};
	}

	BAN::ErrorOr<void> USBMassStorageDriver::clear_feature(uint8_t endpoint_id)
	{
		const uint8_t direction = (endpoint_id % 2) ? 0x80 : 0x00;
		const uint8_t number    =  endpoint_id / 2;

		USBDeviceRequest clear_feature_request {
			.bmRequestType = USB::RequestType::HostToDevice | USB::RequestType::Standard | USB::RequestType::Endpoint,
			.bRequest      = USB::Request::CLEAR_FEATURE,
			.wValue        = 0x0000,
			.wIndex        = static_cast<uint16_t>(direction | number),
			.wLength       = 0x0000,
		};

		TRY(m_device.send_request(clear_feature_request, 0));
		return {};
	}

	BAN::ErrorOr<void> USBMassStorageDriver::reset_recovery()
	{
		TRY(mass_storage_reset());
		TRY(clear_feature(m_in_endpoint_id));
		TRY(clear_feature(m_out_endpoint_id));
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

		const uint64_t timeout_ms = SystemTimer::get().ms_since_boot() + s_timeout_ms;
		while (bytes_sent == invalid)
		{
			if (SystemTimer::get().ms_since_boot() < timeout_ms)
				continue;
			if (reset_recovery().is_error())
				dwarnln_if(DEBUG_USB_MASS_STORAGE, "could not reset USBMassStorage");
			return BAN::Error::from_errno(EIO);
		}

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

		const uint64_t timeout_ms = SystemTimer::get().ms_since_boot() + s_timeout_ms;
		while (bytes_recv == invalid)
		{
			if (SystemTimer::get().ms_since_boot() < timeout_ms)
				continue;
			if (reset_recovery().is_error())
				dwarnln_if(DEBUG_USB_MASS_STORAGE, "could not reset USBMassStorage");
			return BAN::Error::from_errno(EIO);
		}

		m_in_callback.clear();

		return static_cast<size_t>(bytes_recv);
	}

	template<bool IN, typename SPAN>
	BAN::ErrorOr<size_t> USBMassStorageDriver::send_command(uint8_t lun, BAN::ConstByteSpan command, SPAN data)
	{
		ASSERT(command.size() <= 16);

		LockGuard _(m_mutex);

		auto& cbw = *reinterpret_cast<USBMassStorage::CBW*>(m_data_region->vaddr());
		cbw = {
			.dCBWSignature          = 0x43425355,
			.dCBWTag                = 0x00000000,
			.dCBWDataTransferLength = static_cast<uint32_t>(data.size()),
			.bmCBWFlags             = IN ? 0x80 : 0x00,
			.bCBWLUN                = lun,
			.bCBWCBLength           = static_cast<uint8_t>(command.size()),
			.CBWCB                  = {},
		};
		memcpy(cbw.CBWCB, command.data(), command.size());

		if (TRY(send_bytes(m_data_region->paddr(), sizeof(USBMassStorage::CBW))) != sizeof(USBMassStorage::CBW))
		{
			dwarnln("failed to send CBW");
			return BAN::Error::from_errno(EIO);
		}

		const size_t ntransfer =
			TRY([&]() -> BAN::ErrorOr<size_t>
			{
				if (data.empty())
					return 0;
				if constexpr (IN)
					return TRY(recv_bytes(m_data_region->paddr(), data.size()));
				memcpy(reinterpret_cast<void*>(m_data_region->vaddr()), data.data(), data.size());
				return TRY(send_bytes(m_data_region->paddr(), data.size()));
			}());

		if (ntransfer == transfer_stall)
			TRY(clear_feature(IN ? m_in_endpoint_id : m_out_endpoint_id));

		if constexpr (IN)
			memcpy(data.data(), reinterpret_cast<void*>(m_data_region->vaddr()), ntransfer);

		size_t csw_ntransfer = TRY(recv_bytes(m_data_region->paddr(), sizeof(USBMassStorage::CSW)));
		if (csw_ntransfer == transfer_stall)
		{
			TRY(clear_feature(m_in_endpoint_id));
			csw_ntransfer = TRY(recv_bytes(m_data_region->paddr(), sizeof(USBMassStorage::CSW)));
		}

		if (csw_ntransfer != sizeof(USBMassStorage::CSW))
		{
			dwarnln("could not receive CSW");
			return BAN::Error::from_errno(EFAULT);
		}

		const auto& csw = *reinterpret_cast<USBMassStorage::CSW*>(m_data_region->vaddr());
		switch (csw.bmCSWStatus)
		{
			case 0x00:
			case 0x01:
				return data.size() - csw.dCSWDataResidue;
			default:
				dwarnln_if(DEBUG_USB_MASS_STORAGE, "received invalid CSW");
				// fall through
			case 0x02:
				TRY(reset_recovery());
				return BAN::Error::from_errno(EIO);
		}

		ASSERT_NOT_REACHED();
	}

	template BAN::ErrorOr<size_t> USBMassStorageDriver::send_command<true,  BAN::ByteSpan     >(uint8_t, BAN::ConstByteSpan, BAN::ByteSpan);
	template BAN::ErrorOr<size_t> USBMassStorageDriver::send_command<false, BAN::ConstByteSpan>(uint8_t, BAN::ConstByteSpan, BAN::ConstByteSpan);

	void USBMassStorageDriver::handle_stall(uint8_t endpoint_id)
	{
		if (endpoint_id != m_in_endpoint_id && endpoint_id != m_out_endpoint_id)
			return;

		dprintln_if(DEBUG_USB_MASS_STORAGE, "got STALL to {} endpoint", endpoint_id == m_in_endpoint_id ? "IN" : "OUT");

		if (m_in_endpoint_id == endpoint_id)
		{
			ASSERT(m_in_callback);
			return m_in_callback(transfer_stall);
		}

		if (m_out_endpoint_id == endpoint_id)
		{
			ASSERT(m_out_callback);
			return m_out_callback(transfer_stall);
		}
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
