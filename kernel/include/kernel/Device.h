#pragma once

#include <BAN/Vector.h>
#include <kernel/SpinLock.h>

namespace Kernel
{

	class Device
	{
	public:
		enum class Type
		{
			BlockDevice,
			CharacterDevice,
			DeviceController
		};

		virtual ~Device() {}
		virtual Type type() const = 0;
		virtual void update() {}
	};

	class BlockDevice : public Device
	{
	public:
		virtual Type type() const override { return Type::BlockDevice; }
	};

	class CharacterDevice : public Device
	{
	public:
		virtual Type type() const override { return Type::CharacterDevice; }

		virtual BAN::ErrorOr<void> read(BAN::Span<uint8_t>);
	};

	class DeviceManager
	{
		BAN_NON_COPYABLE(DeviceManager);
		BAN_NON_MOVABLE(DeviceManager);

	public:
		static DeviceManager& get();

		void update();
		void add_device(Device*);

		BAN::Vector<Device*> devices() { return m_devices; }

	private:
		DeviceManager() = default;

	private:
		SpinLock m_lock;
		BAN::Vector<Device*> m_devices;
	};

}