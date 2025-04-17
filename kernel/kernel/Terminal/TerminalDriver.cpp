#include <kernel/BootInfo.h>
#include <kernel/Terminal/FramebufferTerminal.h>
#include <kernel/Terminal/TextModeTerminal.h>

namespace Kernel
{

	BAN::RefPtr<TerminalDriver> g_terminal_driver;

	BAN::ErrorOr<void> TerminalDriver::initialize_from_boot_info()
	{
		switch (g_boot_info.framebuffer.type)
		{
			case FramebufferInfo::Type::None:
			case FramebufferInfo::Type::Unknown:
				return BAN::Error::from_errno(ENODEV);
			case FramebufferInfo::Type::RGB:
				g_terminal_driver = TRY(FramebufferTerminalDriver::create(
					TRY(FramebufferDevice::create_from_boot_framebuffer())
				));
				break;
			case FramebufferInfo::Type::Text:
				g_terminal_driver = TRY(TextModeTerminalDriver::create_from_boot_info());
				break;
		}
		return {};
	}

}
