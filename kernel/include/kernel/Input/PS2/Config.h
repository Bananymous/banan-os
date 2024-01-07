#pragma once

#include <stdint.h>

namespace Kernel::Input::PS2
{

	enum IOPort : uint8_t
	{
		DATA = 0x60,
		STATUS = 0x64,
		COMMAND = 0x64,
	};

	enum Status : uint8_t
	{
		OUTPUT_STATUS = (1 << 0),
		INPUT_STATUS = (1 << 1),
		SYSTEM = (1 << 2),
		DEVICE_OR_CONTROLLER = (1 << 3),
		TIMEOUT_ERROR = (1 << 6),
		PARITY_ERROR = (1 << 7),
	};

	enum Config : uint8_t
	{
		INTERRUPT_FIRST_PORT = (1 << 0),
		INTERRUPT_SECOND_PORT = (1 << 1),
		SYSTEM_FLAG = (1 << 2),
		ZERO1 = (1 << 3),
		CLOCK_FIRST_PORT = (1 << 4),
		CLOCK_SECOND_PORT = (1 << 5),
		TRANSLATION_FIRST_PORT = (1 << 6),
		ZERO2 = (1 << 7),
	};

	enum Command : uint8_t
	{
		READ_CONFIG = 0x20,
		WRITE_CONFIG = 0x60,
		DISABLE_SECOND_PORT = 0xA7,
		ENABLE_SECOND_PORT = 0xA8,
		TEST_SECOND_PORT = 0xA9,
		TEST_CONTROLLER = 0xAA,
		TEST_FIRST_PORT = 0xAB,
		DISABLE_FIRST_PORT = 0xAD,
		ENABLE_FIRST_PORT = 0xAE,
		WRITE_TO_SECOND_PORT = 0xD4,
	};

	enum Response : uint8_t
	{
		TEST_FIRST_PORT_PASS = 0x00,
		TEST_SECOND_PORT_PASS = 0x00,
		TEST_CONTROLLER_PASS = 0x55,
		SELF_TEST_PASS = 0xAA,
		ACK = 0xFA,
		RESEND = 0xFE,
	};

	enum DeviceCommand : uint8_t
	{
		ENABLE_SCANNING = 0xF4,
		DISABLE_SCANNING = 0xF5,
		IDENTIFY = 0xF2,
		RESET = 0xFF,
	};

	enum IRQ : uint8_t
	{
		DEVICE0 = 1,
		DEVICE1 = 12,
	};

	enum KBResponse : uint8_t
	{
		KEY_ERROR_OR_BUFFER_OVERRUN1 = 0x00,
		KEY_ERROR_OR_BUFFER_OVERRUN2 = 0xFF,
	};

	enum KBScancode : uint8_t
	{
		SET_SCANCODE_SET1 = 1,
		SET_SCANCODE_SET2 = 2,
		SET_SCANCODE_SET3 = 3,
	};

	enum KBLeds : uint8_t
	{
		SCROLL_LOCK	= (1 << 0),
		NUM_LOCK	= (1 << 1),
		CAPS_LOCK	= (1 << 2),
	};

}
