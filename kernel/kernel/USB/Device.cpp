#include <kernel/Memory/DMARegion.h>
#include <kernel/USB/Device.h>
#include <kernel/USB/HID/HIDDriver.h>
#include <kernel/USB/Hub/HubDriver.h>
#include <kernel/USB/MassStorage/MassStorageDriver.h>

#define USB_DUMP_DESCRIPTORS 0

namespace Kernel
{

	BAN::ErrorOr<void> USBDevice::initialize()
	{
		dprintln_if(DEBUG_USB, "initializing control endpoint");

		TRY(initialize_control_endpoint());

		m_dma_buffer = TRY(DMARegion::create(1024));

		dprintln_if(DEBUG_USB, "getting device descriptor");

		USBDeviceRequest request;
		request.bmRequestType = USB::RequestType::DeviceToHost | USB::RequestType::Standard | USB::RequestType::Device;
		request.bRequest      = USB::Request::GET_DESCRIPTOR;
		request.wValue        = static_cast<uint16_t>(USB::DescriptorType::DEVICE) << 8;
		request.wIndex        = 0;
		request.wLength       = sizeof(USBDeviceDescriptor);
		auto transferred = TRY(send_request(request, m_dma_buffer->paddr()));

		m_descriptor.descriptor = *reinterpret_cast<const USBDeviceDescriptor*>(m_dma_buffer->vaddr());
		if (transferred < sizeof(USBDeviceDescriptor) || transferred < m_descriptor.descriptor.bLength)
		{
			dprintln("invalid device descriptor response {}");
			return BAN::Error::from_errno(EINVAL);
		}

		dprintln_if(DEBUG_USB, "device has {} configurations", m_descriptor.descriptor.bNumConfigurations);

		for (uint32_t i = 0; i < m_descriptor.descriptor.bNumConfigurations; i++)
			if (auto opt_configuration = parse_configuration(i); !opt_configuration.is_error())
				TRY(m_descriptor.configurations.push_back(opt_configuration.release_value()));

#if USB_DUMP_DESCRIPTORS
		const auto& descriptor = m_descriptor.descriptor;
		dprintln("device descriptor");
		dprintln("  bLength:            {}", descriptor.bLength);
		dprintln("  bDescriptorType:    {}", descriptor.bDescriptorType);
		dprintln("  bcdUSB:             {}", descriptor.bcdUSB);
		dprintln("  bDeviceClass:       {}", descriptor.bDeviceClass);
		dprintln("  bDeviceSubClass:    {}", descriptor.bDeviceSubClass);
		dprintln("  bDeviceProtocol:    {}", descriptor.bDeviceProtocol);
		dprintln("  bMaxPacketSize0:    {}", descriptor.bMaxPacketSize0);
		dprintln("  idVendor:           {}", descriptor.idVendor);
		dprintln("  idProduct:          {}", descriptor.idProduct);
		dprintln("  bcdDevice:          {}", descriptor.bcdDevice);
		dprintln("  iManufacturer:      {}", descriptor.iManufacturer);
		dprintln("  iProduct:           {}", descriptor.iProduct);
		dprintln("  iSerialNumber:      {}", descriptor.iSerialNumber);
		dprintln("  bNumConfigurations: {}", descriptor.bNumConfigurations);

		for (const auto& configuration : m_descriptor.configurations)
		{
			const auto& descriptor = configuration.desciptor;
			dprintln("  configuration");
			dprintln("    bLength:             {}", descriptor.bLength);
			dprintln("    bDescriptorType:     {}", descriptor.bDescriptorType);
			dprintln("    wTotalLength:        {}", descriptor.wTotalLength);
			dprintln("    bNumInterfaces:      {}", descriptor.bNumInterfaces);
			dprintln("    bConfigurationValue: {}", descriptor.bConfigurationValue);
			dprintln("    iConfiguration:      {}", descriptor.iConfiguration);
			dprintln("    bmAttributes:        {}", descriptor.bmAttributes);
			dprintln("    bMaxPower:           {}", descriptor.bMaxPower);

			for (const auto& interface : configuration.interfaces)
			{
				const auto& descriptor = interface.descriptor;
				dprintln("    interface");
				dprintln("      bLength:            {}", descriptor.bLength);
				dprintln("      bDescriptorType:    {}", descriptor.bDescriptorType);
				dprintln("      bInterfaceNumber:   {}", descriptor.bInterfaceNumber);
				dprintln("      bAlternateSetting:  {}", descriptor.bAlternateSetting);
				dprintln("      bNumEndpoints:      {}", descriptor.bNumEndpoints);
				dprintln("      bInterfaceClass:    {}", descriptor.bInterfaceClass);
				dprintln("      bInterfaceSubClass: {}", descriptor.bInterfaceSubClass);
				dprintln("      bInterfaceProtocol: {}", descriptor.bInterfaceProtocol);
				dprintln("      iInterface:         {}", descriptor.iInterface);

				for (const auto& endpoint : interface.endpoints)
				{
					const auto& descriptor = endpoint.descriptor;
					dprintln("      endpoint");
					dprintln("        bLength:          {}", descriptor.bLength);
					dprintln("        bDescriptorType:  {}", descriptor.bDescriptorType);
					dprintln("        bEndpointAddress: {}", descriptor.bEndpointAddress);
					dprintln("        bmAttributes:     {}", descriptor.bmAttributes);
					dprintln("        wMaxPacketSize:   {}", +descriptor.wMaxPacketSize);
					dprintln("        bInterval:        {}", descriptor.bInterval);
				}
			}
		}
#endif

		if (m_descriptor.descriptor.bDeviceClass)
		{
			switch (static_cast<USB::DeviceBaseClass>(m_descriptor.descriptor.bDeviceClass))
			{
				case USB::DeviceBaseClass::CommunicationAndCDCControl:
					dprintln_if(DEBUG_USB, "Found CommunicationAndCDCControl device");
					return BAN::Error::from_errno(ENOTSUP);
				case USB::DeviceBaseClass::Hub:
				{
					if (auto ret = BAN::UniqPtr<USBHubDriver>::create(*this, m_descriptor); !ret.is_error())
						TRY(m_class_drivers.push_back(ret.release_value()));
					else
					{
						dwarnln("Failed to create USB Hub: {}", ret.error());
						return ret.release_error();
					}

					if (auto ret = m_class_drivers[0]->initialize(); ret.is_error())
					{
						dwarnln("Failed to initialize USB Hub: {}", ret.error());
						m_class_drivers.clear();
						return ret.release_error();
					}

					dprintln_if(DEBUG_USB, "Successfully initialized USB hub");

					return {};
				}
				case USB::DeviceBaseClass::BillboardDeviceClass:
					dprintln_if(DEBUG_USB, "Found BillboardDeviceClass device");
					return BAN::Error::from_errno(ENOTSUP);
				case USB::DeviceBaseClass::DiagnosticDevice:
					dprintln_if(DEBUG_USB, "Found DiagnosticDevice device");
					return BAN::Error::from_errno(ENOTSUP);
				case USB::DeviceBaseClass::Miscellaneous:
					dprintln_if(DEBUG_USB, "Found Miscellaneous device");
					return BAN::Error::from_errno(ENOTSUP);
				case USB::DeviceBaseClass::VendorSpecific:
					dprintln_if(DEBUG_USB, "Found VendorSpecific device");
					return BAN::Error::from_errno(ENOTSUP);
				default:
					dprintln_if(DEBUG_USB, "Invalid device base class {2H}", m_descriptor.descriptor.bDeviceClass);
					return BAN::Error::from_errno(EFAULT);
			}
			ASSERT_NOT_REACHED();
		}

		for (size_t i = 0; i < m_descriptor.configurations.size(); i++)
		{
			const auto& configuration = m_descriptor.configurations[i];

			{
				dprintln_if(DEBUG_USB, "Setting configuration 0x{2H}", configuration.desciptor.bConfigurationValue);

				USBDeviceRequest request;
				request.bmRequestType = USB::RequestType::HostToDevice | USB::RequestType::Standard | USB::RequestType::Device;
				request.bRequest      = USB::Request::SET_CONFIGURATION;
				request.wValue        = configuration.desciptor.bConfigurationValue;
				request.wIndex        = 0;
				request.wLength       = 0;
				TRY(send_request(request, 0));
			}

			for (const auto& interface : configuration.interfaces)
			{
				// FIXME: support alternate settings
				if (interface.descriptor.bAlternateSetting != 0)
					continue;

				switch (static_cast<USB::InterfaceBaseClass>(interface.descriptor.bInterfaceClass))
				{
					case USB::InterfaceBaseClass::Audio:
						dprintln_if(DEBUG_USB, "Found Audio interface");
						break;
					case USB::InterfaceBaseClass::CommunicationAndCDCControl:
						dprintln_if(DEBUG_USB, "Found CommunicationAndCDCControl interface");
						break;
					case USB::InterfaceBaseClass::HID:
						if (auto result = BAN::UniqPtr<USBHIDDriver>::create(*this, interface); !result.is_error())
							TRY(m_class_drivers.push_back(result.release_value()));
						break;
					case USB::InterfaceBaseClass::Physical:
						dprintln_if(DEBUG_USB, "Found Physical interface");
						break;
					case USB::InterfaceBaseClass::Image:
						dprintln_if(DEBUG_USB, "Found Image interface");
						break;
					case USB::InterfaceBaseClass::Printer:
						dprintln_if(DEBUG_USB, "Found Printer interface");
						break;
					case USB::InterfaceBaseClass::MassStorage:
						if (auto result = BAN::UniqPtr<USBMassStorageDriver>::create(*this, interface); !result.is_error())
							TRY(m_class_drivers.push_back(result.release_value()));
						break;
					case USB::InterfaceBaseClass::CDCData:
						dprintln_if(DEBUG_USB, "Found CDCData interface");
						break;
					case USB::InterfaceBaseClass::SmartCard:
						dprintln_if(DEBUG_USB, "Found SmartCard interface");
						break;
					case USB::InterfaceBaseClass::ContentSecurity:
						dprintln_if(DEBUG_USB, "Found ContentSecurity interface");
						break;
					case USB::InterfaceBaseClass::Video:
						dprintln_if(DEBUG_USB, "Found Video interface");
						break;
					case USB::InterfaceBaseClass::PersonalHealthcare:
						dprintln_if(DEBUG_USB, "Found PersonalHealthcare interface");
						break;
					case USB::InterfaceBaseClass::AudioVideoDevice:
						dprintln_if(DEBUG_USB, "Found AudioVideoDevice interface");
						break;
					case USB::InterfaceBaseClass::USBTypeCBridgeClass:
						dprintln_if(DEBUG_USB, "Found USBTypeCBridgeClass interface");
						break;
					case USB::InterfaceBaseClass::USBBulkDisplayProtocolDeviceClass:
						dprintln_if(DEBUG_USB, "Found USBBulkDisplayProtocolDeviceClass interface");
						break;
					case USB::InterfaceBaseClass::MCTPOverUSBProtocolEndpointDeviceClass:
						dprintln_if(DEBUG_USB, "Found MCTPOverUSBProtocolEndpointDeviceClass interface");
						break;
					case USB::InterfaceBaseClass::I3CDeviceClass:
						dprintln_if(DEBUG_USB, "Found I3CDeviceClass interface");
						break;
					case USB::InterfaceBaseClass::DiagnosticDevice:
						dprintln_if(DEBUG_USB, "Found DiagnosticDevice interface");
						break;
					case USB::InterfaceBaseClass::WirelessController:
						dprintln_if(DEBUG_USB, "Found WirelessController interface");
						break;
					case USB::InterfaceBaseClass::Miscellaneous:
						dprintln_if(DEBUG_USB, "Found Miscellaneous interface");
						break;
					case USB::InterfaceBaseClass::ApplicationSpecific:
						dprintln_if(DEBUG_USB, "Found ApplicationSpecific interface");
						break;
					case USB::InterfaceBaseClass::VendorSpecific:
						dprintln_if(DEBUG_USB, "Found VendorSpecific interface");
						break;
					default:
						dprintln_if(DEBUG_USB, "Invalid interface base class {2H}", interface.descriptor.bInterfaceClass);
						break;
				}
			}

			for (size_t i = 0; i < m_class_drivers.size(); i++)
			{
				if (auto ret = m_class_drivers[i]->initialize(); ret.is_error())
				{
					dwarnln("Could not initialize USB interface {}", ret.error());
					m_class_drivers.remove(i--);
				}
			}

			if (!m_class_drivers.empty())
			{
				dprintln("Successfully initialized USB device with {}/{} interfaces",
					m_class_drivers.size(),
					configuration.interfaces.size()
				);
				return {};
			}
		}

		return BAN::Error::from_errno(ENOTSUP);
	}

	BAN::ErrorOr<USBDevice::ConfigurationDescriptor> USBDevice::parse_configuration(size_t index)
	{
		{
			USBDeviceRequest request;
			request.bmRequestType = USB::RequestType::DeviceToHost | USB::RequestType::Standard | USB::RequestType::Device;
			request.bRequest      = USB::Request::GET_DESCRIPTOR;
			request.wValue        = (static_cast<uint16_t>(USB::DescriptorType::CONFIGURATION) << 8) | index;
			request.wIndex        = 0;
			request.wLength       = m_dma_buffer->size();
			auto transferred = TRY(send_request(request, m_dma_buffer->paddr()));

			if (transferred < sizeof(USBConfigurationDescriptor))
			{
				dwarnln("usb device responded with only {} bytes of configuration", transferred);
				return BAN::Error::from_errno(EINVAL);
			}

			auto configuration = *reinterpret_cast<const USBConfigurationDescriptor*>(m_dma_buffer->vaddr());

			dprintln_if(DEBUG_USB, "configuration {} is {} bytes", index, +configuration.wTotalLength);
			if (configuration.bLength < sizeof(USBConfigurationDescriptor) || transferred < configuration.wTotalLength)
			{
				dwarnln("invalid configuration descriptor size: {} length, {} total", configuration.bLength, +configuration.wTotalLength);
				return BAN::Error::from_errno(EINVAL);
			}
		}

		ConfigurationDescriptor configuration;
		configuration.desciptor = *reinterpret_cast<const USBConfigurationDescriptor*>(m_dma_buffer->vaddr());

		ptrdiff_t offset = configuration.desciptor.bLength;
		while (offset < configuration.desciptor.wTotalLength)
		{
			const uint8_t length = *reinterpret_cast<const uint8_t*>(m_dma_buffer->vaddr() + offset + 0);
			const uint8_t type   = *reinterpret_cast<const uint8_t*>(m_dma_buffer->vaddr() + offset + 1);

			switch (type)
			{
				case USB::DescriptorType::INTERFACE:
					if (length < sizeof(USBInterfaceDescriptor))
					{
						dwarnln("invalid interface descriptor size {}", length);
						return BAN::Error::from_errno(EINVAL);
					}
					TRY(configuration.interfaces.emplace_back(
						*reinterpret_cast<const USBInterfaceDescriptor*>(m_dma_buffer->vaddr() + offset),
						BAN::Vector<EndpointDescriptor>(),
						BAN::Vector<BAN::Vector<uint8_t>>()
					));
					break;
				case USB::DescriptorType::ENDPOINT:
					if (length < sizeof(USBEndpointDescriptor))
					{
						dwarnln("invalid interface descriptor size {}", length);
						return BAN::Error::from_errno(EINVAL);
					}
					if (configuration.interfaces.empty())
					{
						dwarnln("invalid endpoint descriptor before interface descriptor");
						return BAN::Error::from_errno(EINVAL);
					}
					TRY(configuration.interfaces.back().endpoints.emplace_back(
						*reinterpret_cast<const USBEndpointDescriptor*>(m_dma_buffer->vaddr() + offset)
					));
					break;
				default:
					if (configuration.interfaces.empty())
						dprintln_if(DEBUG_USB, "skipping descriptor type {}", type);
					else
					{
						BAN::Vector<uint8_t> descriptor;
						TRY(descriptor.resize(length));
						memcpy(descriptor.data(), reinterpret_cast<const void*>(m_dma_buffer->vaddr() + offset), length);
						TRY(configuration.interfaces.back().misc_descriptors.push_back(BAN::move(descriptor)));
					}
					break;
			}

			offset += length;
		}

		return BAN::move(configuration);
	}

	void USBDevice::handle_stall(uint8_t endpoint_id)
	{
		for (auto& driver : m_class_drivers)
			driver->handle_stall(endpoint_id);
	}

	void USBDevice::handle_input_data(size_t byte_count, uint8_t endpoint_id)
	{
		for (auto& driver : m_class_drivers)
			driver->handle_input_data(byte_count, endpoint_id);
	}

}
