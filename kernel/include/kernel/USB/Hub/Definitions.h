#pragma once

#include <stdint.h>

namespace Kernel::USBHub
{

	struct HubDescriptor
	{
		uint8_t bLength;
		uint8_t bDescriptorType;
		uint8_t bNbrPorts;
		uint16_t wHubCharacteristics;
		uint8_t bPowerOnGood;
		uint8_t bHubContrCurrent;
		uint8_t bitmaps[];
	} __attribute__((packed));
	static_assert(sizeof(HubDescriptor) == 7);

	struct PortStatus
	{
		union
		{
			uint16_t raw;
			struct {
				uint16_t connection   : 1; // bit 0
				uint16_t enable       : 1; // bit 1
				uint16_t              : 1;
				uint16_t over_current : 1; // bit 3
				uint16_t reset        : 1; // bit 4
			};
			struct {
				uint16_t              : 2;
				uint16_t suspend      : 1; // bit 2
				uint16_t              : 5;
				uint16_t power        : 1; // bit 8
				uint16_t low_speed    : 1; // bit 9
				uint16_t high_speed   : 1; // bit 10
			} usb2;
			struct {
				uint16_t              : 5;
				uint16_t link_state   : 4; // bit 5-8
				uint16_t power        : 1; // bit 9
				uint16_t speed        : 3; // bit 10-12
			} usb3;
		} wPortStatus;
		union
		{
			uint16_t raw;
			struct {
				uint16_t connection   : 1; // bit 0
				uint16_t              : 2;
				uint16_t over_current : 1; // bit 3
				uint16_t reset        : 1; // bit 4
			};
			struct {
				uint16_t              : 1;
				uint16_t enable       : 1; // bit 1
				uint16_t suspend      : 1; // bit 2
			} usb2;
			struct {
				uint16_t              : 5;
				uint16_t bh_reset     : 1; // bit 5
				uint16_t link_state   : 1; // bit 6
				uint16_t config_error : 1; // bit 7
			} usb3;
		} wPortChanged;
	};
	static_assert(sizeof(PortStatus) == 4);

	enum HubSelector
	{
		C_HUB_LOCAL_POWER = 0,
		C_HUB_OVER_CURRENT = 1,
		PORT_CONNECTION = 0,
		PORT_ENABLE = 1,
		PORT_SUSPEND = 2,
		PORT_OVER_CURRENT = 3,
		PORT_RESET = 4,
		PORT_LINK_STATE = 5,
		PORT_POWER = 8,
		PORT_LOW_SPEED = 9,
		C_PORT_CONNECTION = 16,
		C_PORT_ENABLE = 17,
		C_PORT_SUSPEND = 18,
		C_PORT_OVER_CURRENT = 19,
		C_PORT_RESET = 20,
		PORT_TEST = 21,
		PORT_INDICATOR = 22,
		C_PORT_LINK_STATE = 25,
		C_PORT_CONFIG_ERROR = 26,
		C_BH_PORT_RESET = 29,
	};

}
