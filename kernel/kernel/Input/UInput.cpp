#include <kernel/FS/DevFS/FileSystem.h>
#include <kernel/Input/UInput.h>
#include <kernel/Process.h>
#include <kernel/UserCopy.h>

#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/uinput.h>

namespace Kernel
{

	BAN::ErrorOr<BAN::RefPtr<UInputControl>> UInputControl::create(mode_t mode, uid_t uid, gid_t gid)
	{
		auto* uinput_ptr = new UInputControl(mode, uid, gid);
		if (uinput_ptr == nullptr)
			return BAN::Error::from_errno(ENOMEM);
		return BAN::RefPtr<UInputControl>::adopt(uinput_ptr);
	}

	UInputControl::UInputControl(mode_t mode, uid_t uid, gid_t gid)
		: CharacterDevice(mode, uid, gid)
	{}

	BAN::ErrorOr<long> UInputControl::ioctl_impl(unsigned long request, void* argument)
	{
		if (request != UINPUT_DEVICE_CREATE)
			return CharacterDevice::ioctl_impl(request, argument);

		uinput_device device;
		TRY(read_from_user(argument, &device, sizeof(device)));

		InputDevice::Type type;
		switch (device.type)
		{
			case UINPUT_DEVICE_TYPE_KEYBOARD: type = InputDevice::Type::Keyboard; break;
			case UINPUT_DEVICE_TYPE_MOUSE:    type = InputDevice::Type::Mouse;    break;
			case UINPUT_DEVICE_TYPE_JOYSTICK: type = InputDevice::Type::Joystick; break;
			default:
				return BAN::Error::from_errno(EINVAL);
		}

		auto slave  = TRY(UInputSlave::create(type));
		auto master = TRY(UInputMaster::create(slave));

		DevFileSystem::get().remove_from_cache(master);
		DevFileSystem::get().add_device(slave);

		using File = VirtualFileSystem::File;
		return TRY(Process::current().open_inode(File(master, master->name()), O_RDWR));
	}



	BAN::ErrorOr<BAN::RefPtr<UInputMaster>> UInputMaster::create(BAN::RefPtr<UInputSlave> slave)
	{
		auto* master_ptr = new UInputMaster(slave);
		if (master_ptr == nullptr)
			return BAN::Error::from_errno(ENOMEM);
		return BAN::RefPtr<UInputMaster>::adopt(master_ptr);
	}

	UInputMaster::UInputMaster(BAN::RefPtr<UInputSlave> slave)
		: CharacterDevice(0600, 0, 0)
		, m_slave(slave)
	{ }

	UInputMaster::~UInputMaster()
	{
		DevFileSystem::get().remove_device(m_slave);
	}

	BAN::ErrorOr<size_t> UInputMaster::write_impl(off_t, BAN::ConstByteSpan buffer)
	{
		return m_slave->on_master_write(buffer);
	}



	BAN::ErrorOr<BAN::RefPtr<UInputSlave>> UInputSlave::create(InputDevice::Type type)
	{
		auto* slave_ptr = new UInputSlave(type);
		if (slave_ptr == nullptr)
			return BAN::Error::from_errno(ENOMEM);
		return BAN::RefPtr<UInputSlave>::adopt(slave_ptr);
	}

	UInputSlave::UInputSlave(InputDevice::Type type)
		: InputDevice(type)
	{ }

	BAN::ErrorOr<size_t> UInputSlave::on_master_write(BAN::ConstByteSpan buffer)
	{
		if (buffer.size() != m_event_size)
			return BAN::Error::from_errno(EINVAL);
		add_event(buffer);
		return buffer.size();
	}

}
