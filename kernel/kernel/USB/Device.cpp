#include <kernel/Memory/DMARegion.h>
#include <kernel/USB/Device.h>

#define DEBUG_USB 0
#define USB_DUMP_DESCRIPTORS 0

namespace Kernel
{

	BAN::ErrorOr<void> USBDevice::initialize()
	{
		TRY(initialize_control_endpoint());

		auto buffer = TRY(DMARegion::create(PAGE_SIZE));

		USBDeviceRequest request;
		request.bmRequestType = USB::RequestType::DeviceToHost | USB::RequestType::Standard | USB::RequestType::Device;
		request.bRequest      = USB::Request::GET_DESCRIPTOR;
		request.wValue        = 0x0100;
		request.wIndex        = 0;
		request.wLength       = sizeof(USBDeviceDescriptor);
		TRY(send_request(request, buffer->paddr()));

		m_descriptor.descriptor = *reinterpret_cast<const USBDeviceDescriptor*>(buffer->vaddr());
		dprintln_if(DEBUG_USB, "device has {} configurations", m_descriptor.descriptor.bNumConfigurations);

		for (uint32_t i = 0; i < m_descriptor.descriptor.bNumConfigurations; i++)
		{
			{
				USBDeviceRequest request;
				request.bmRequestType = USB::RequestType::DeviceToHost | USB::RequestType::Standard | USB::RequestType::Device;
				request.bRequest      = USB::Request::GET_DESCRIPTOR;
				request.wValue        = 0x0200 | i;
				request.wIndex        = 0;
				request.wLength       = sizeof(USBConfigurationDescriptor);
				TRY(send_request(request, buffer->paddr()));

				auto configuration = *reinterpret_cast<const USBConfigurationDescriptor*>(buffer->vaddr());

				dprintln_if(DEBUG_USB, "  configuration {} is {} bytes", i, +configuration.wTotalLength);
				if (configuration.wTotalLength > buffer->size())
				{
					dwarnln("  our buffer is only {} bytes, skipping some fields...");
					configuration.wTotalLength = buffer->size();
				}

				if (configuration.wTotalLength > request.wLength)
				{
					request.wLength = configuration.wTotalLength;
					TRY(send_request(request, buffer->paddr()));
				}
			}

			auto configuration = *reinterpret_cast<const USBConfigurationDescriptor*>(buffer->vaddr());

			BAN::Vector<InterfaceDescriptor> interfaces;
			TRY(interfaces.reserve(configuration.bNumInterfaces));

			dprintln_if(DEBUG_USB, "  configuration {} has {} interfaces", i, configuration.bNumInterfaces);

			uintptr_t offset = configuration.bLength;
			for (uint32_t j = 0; j < configuration.bNumInterfaces; j++)
			{
				if (offset + sizeof(USBInterfaceDescritor) > buffer->size())
					break;
				auto interface = *reinterpret_cast<const USBInterfaceDescritor*>(buffer->vaddr() + offset);

				BAN::Vector<EndpointDescriptor> endpoints;
				TRY(endpoints.reserve(interface.bNumEndpoints));

				dprintln_if(DEBUG_USB, "    interface {} has {} endpoints", j, interface.bNumEndpoints);

				offset += interface.bLength;
				for (uint32_t k = 0; k < interface.bNumEndpoints; k++)
				{
					if (offset + sizeof(USBEndpointDescriptor) > buffer->size())
						break;
					auto endpoint = *reinterpret_cast<const USBEndpointDescriptor*>(buffer->vaddr() + offset);
					offset += endpoint.bLength;

					TRY(endpoints.emplace_back(endpoint));
				}

				TRY(interfaces.emplace_back(interface, BAN::move(endpoints)));
			}

			TRY(m_descriptor.configurations.emplace_back(configuration, BAN::move(interfaces)));
		}

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
					dprintln_if(DEBUG_USB, "Found Hub device");
					return BAN::Error::from_errno(ENOTSUP);
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

		for (const auto& configuration : m_descriptor.configurations)
		{
			for (const auto& interface : configuration.interfaces)
			{
				switch (static_cast<USB::InterfaceBaseClass>(interface.descriptor.bInterfaceClass))
				{
					case USB::InterfaceBaseClass::Audio:
						dprintln_if(DEBUG_USB, "Found Audio interface");
						break;
					case USB::InterfaceBaseClass::CommunicationAndCDCControl:
						dprintln_if(DEBUG_USB, "Found CommunicationAndCDCControl interface");
						break;
					case USB::InterfaceBaseClass::HID:
						dprintln_if(DEBUG_USB, "Found HID interface");
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
						dprintln_if(DEBUG_USB, "Found MassStorage interface");
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
		}

		return BAN::Error::from_errno(ENOTSUP);
	}

	USB::SpeedClass USBDevice::determine_speed_class(uint64_t bits_per_second)
	{
		if (bits_per_second <= 1'500'000)
			return USB::SpeedClass::LowSpeed;
		if (bits_per_second <= 12'000'000)
			return USB::SpeedClass::FullSpeed;
		else if (bits_per_second <= 480'000'000)
			return USB::SpeedClass::HighSpeed;
		return USB::SpeedClass::SuperSpeed;
	}

}
