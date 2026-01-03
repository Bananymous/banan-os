#include <kernel/Input/InputDevice.h>
#include <kernel/USB/HID/Joystick.h>

namespace Kernel
{

	USBJoystick::USBJoystick(USBHIDDriver& driver)
		: USBHIDDevice(InputDevice::Type::Joystick)
		, m_driver(driver)
	{
	}

	BAN::ErrorOr<void> USBJoystick::initialize()
	{
		// TODO: this is not a generic USB HID joystick driver but one for PS3 controller.
		//       this may still work with other HID joysticks so i won't limit this only
		//       based on the device.

		// linux hid-sony.c

		auto temp_region = TRY(DMARegion::create(17));

		USBDeviceRequest request;

		// move ps3 controller to "operational" state
		request.bmRequestType = USB::RequestType::DeviceToHost | USB::RequestType::Class | USB::RequestType::Interface;
		request.bRequest      = 0x01;
		request.wValue        = 0x03F2;
		request.wIndex        = m_driver.interface().descriptor.bInterfaceNumber;
		request.wLength       = 17;
		TRY(m_driver.device().send_request(request, temp_region->paddr()));

		// some compatible controllers need this too
		request.bmRequestType = USB::RequestType::DeviceToHost | USB::RequestType::Class | USB::RequestType::Interface;
		request.bRequest      = 0x01;
		request.wValue        = 0x03F5;
		request.wIndex        = m_driver.interface().descriptor.bInterfaceNumber;
		request.wLength       = 8;
		TRY(m_driver.device().send_request(request, temp_region->paddr()));

		return {};
	}

	void USBJoystick::start_report()
	{
		m_interrupt_state = m_state_lock.lock();

		for (auto& axis : m_state.axis)
			axis = {};
		for (auto& button : m_state.buttons)
			button = false;
	}

	void USBJoystick::stop_report()
	{
		m_state_lock.unlock(m_interrupt_state);
	}

	void USBJoystick::handle_array(uint16_t usage_page, uint16_t usage)
	{
		(void)usage;
		dprintln("Unsupported array {2H}", usage_page);
	}

	void USBJoystick::handle_variable(uint16_t usage_page, uint16_t usage, int64_t state)
	{
		(void)usage;
		(void)state;
		dprintln("Unsupported relative usage page {2H}", usage_page);
	}

	void USBJoystick::handle_variable_absolute(uint16_t usage_page, uint16_t usage, int64_t state, int64_t min, int64_t max)
	{
		switch (usage_page)
		{
			case 0x01:
				switch (usage)
				{
					case 0x01:
						// TODO: PS3 controller sends some extra data with this usage
						break;
					case 0x30:
						m_state.axis[0] = { state, min, max };
						break;
					case 0x31:
						m_state.axis[1] = { state, min, max };
						break;
					case 0x32:
						m_state.axis[2] = { state, min, max };
						break;
					case 0x35:
						m_state.axis[3] = { state, min, max };
						break;
				}
				break;
			case 0x09:
				if (usage > 0 && usage <= sizeof(m_state.buttons))
					m_state.buttons[usage - 1] = state;
				break;
			default:
				dprintln("Unsupported absolute usage page {2H}", usage_page);
				break;
		}
	}

	BAN::ErrorOr<size_t> USBJoystick::read_impl(off_t, BAN::ByteSpan buffer)
	{
		SpinLockGuard _(m_state_lock);
		const size_t to_copy = BAN::Math::min(buffer.size(), sizeof(m_state));
		memcpy(buffer.data(), &m_state, to_copy);
		return to_copy;
	}

}
