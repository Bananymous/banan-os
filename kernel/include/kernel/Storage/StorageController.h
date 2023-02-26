#pragma once

#include <kernel/PCI.h>
#include <kernel/Storage/StorageDevice.h>

namespace Kernel
{

	class StorageController
	{
	public:
		BAN::Vector<StorageDevice*>& devices() { return m_devices; }
		const BAN::Vector<StorageDevice*>& devices() const { return m_devices; }

	protected:
		BAN::Vector<StorageDevice*> m_devices;
	};

}