#pragma once

#include <stddef.h>
#include <stdint.h>

namespace Kernel
{

	namespace USB
	{
		enum class SpeedClass
		{
			None,
			LowSpeed,
			FullSpeed,
			HighSpeed,
			SuperSpeed,
		};

		enum class DeviceBaseClass : uint8_t
		{
			CommunicationAndCDCControl = 0x02,
			Hub                        = 0x09,
			BillboardDeviceClass       = 0x11,
			DiagnosticDevice           = 0xDC,
			Miscellaneous              = 0xEF,
			VendorSpecific             = 0xFF,
		};

		enum class InterfaceBaseClass : uint8_t
		{
			Audio                                  = 0x01,
			CommunicationAndCDCControl             = 0x02,
			HID                                    = 0x03,
			Physical                               = 0x05,
			Image                                  = 0x06,
			Printer                                = 0x07,
			MassStorage                            = 0x08,
			CDCData                                = 0x0A,
			SmartCard                              = 0x0B,
			ContentSecurity                        = 0x0D,
			Video                                  = 0x0E,
			PersonalHealthcare                     = 0x0F,
			AudioVideoDevice                       = 0x10,
			USBTypeCBridgeClass                    = 0x12,
			USBBulkDisplayProtocolDeviceClass      = 0x13,
			MCTPOverUSBProtocolEndpointDeviceClass = 0x14,
			I3CDeviceClass                         = 0x3C,
			DiagnosticDevice                       = 0xDC,
			WirelessController                     = 0xE0,
			Miscellaneous                          = 0xEF,
			ApplicationSpecific                    = 0xFE,
			VendorSpecific                         = 0xFF,
		};

		enum RequestType : uint8_t
		{
			HostToDevice = 0b0 << 7,
			DeviceToHost = 0b1 << 7,

			Standard = 0b00 << 5,
			Class    = 0b01 << 5,
			Vendor   = 0b10 << 5,

			Device    = 0b00000,
			Interface = 0b00001,
			Endpoint  = 0b00010,
			Other     = 0b00011,
		};

		enum Request : uint8_t
		{
			GET_STATUS        = 0,
			CLEAR_FEATURE     = 1,
			SET_FEATURE       = 3,
			SET_ADDRESS       = 5,
			GET_DESCRIPTOR    = 6,
			SET_DESCRIPTOR    = 7,
			GET_CONFIGURATION = 8,
			SET_CONFIGURATION = 9,
			GET_INTERFACE     = 10,
			SET_INTERFACE     = 11,
			SYNC_FRAME        = 12,
		};
	}

	struct USBDeviceDescriptor
	{
		uint8_t bLength;
		uint8_t bDescriptorType;
		uint16_t bcdUSB;
		uint8_t bDeviceClass;
		uint8_t bDeviceSubClass;
		uint8_t bDeviceProtocol;
		uint8_t bMaxPacketSize0;
		uint16_t idVendor;
		uint16_t idProduct;
		uint16_t bcdDevice;
		uint8_t iManufacturer;
		uint8_t iProduct;
		uint8_t iSerialNumber;
		uint8_t bNumConfigurations;
	};
	static_assert(sizeof(USBDeviceDescriptor) == 18);

	struct USBConfigurationDescriptor
	{
		uint8_t bLength;
		uint8_t bDescriptorType;
		uint16_t wTotalLength __attribute__((packed));
		uint8_t bNumInterfaces;
		uint8_t bConfigurationValue;
		uint8_t iConfiguration;
		uint8_t bmAttributes;
		uint8_t bMaxPower;
	};
	static_assert(sizeof(USBConfigurationDescriptor) == 9);
	static constexpr size_t foo = sizeof(USBConfigurationDescriptor);

	struct USBInterfaceDescritor
	{
		uint8_t bLength;
		uint8_t bDescriptorType;
		uint8_t bInterfaceNumber;
		uint8_t bAlternateSetting;
		uint8_t bNumEndpoints;
		uint8_t bInterfaceClass;
		uint8_t bInterfaceSubClass;
		uint8_t bInterfaceProtocol;
		uint8_t iInterface;
	};
	static_assert(sizeof(USBInterfaceDescritor) == 9);

	struct USBEndpointDescriptor
	{
		uint8_t bLength;
		uint8_t bDescriptorType;
		uint8_t bEndpointAddress;
		uint8_t bmAttributes;
		uint16_t wMaxPacketSize __attribute__((packed));
		uint8_t bInterval;
	};
	static_assert(sizeof(USBEndpointDescriptor) == 7);

	struct USBDeviceRequest
	{
		uint8_t bmRequestType;
		uint8_t bRequest;
		uint16_t wValue;
		uint16_t wIndex;
		uint16_t wLength;
	};
	static_assert(sizeof(USBDeviceRequest) == 8);

}
