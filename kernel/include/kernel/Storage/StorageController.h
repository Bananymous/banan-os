#pragma once

#include <kernel/Storage/StorageDevice.h>

namespace Kernel
{

	class StorageController : public CharacterDevice
	{
	public:
		BAN::Vector<StorageDevice*>& devices() { return m_devices; }
		const BAN::Vector<StorageDevice*>& devices() const { return m_devices; }

	protected:
		void add_device(StorageDevice* device)
		{
			ASSERT(device);
			MUST(m_devices.push_back(device));
		}

	private:
		BAN::Vector<StorageDevice*> m_devices;
	};

}