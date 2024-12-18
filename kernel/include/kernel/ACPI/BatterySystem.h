#pragma once

#include <kernel/ACPI/AML/Namespace.h>
#include <kernel/FS/TmpFS/Inode.h>

namespace Kernel::ACPI
{

	class BatterySystem
	{
		BAN_NON_COPYABLE(BatterySystem);
		BAN_NON_MOVABLE(BatterySystem);
	public:
		static BAN::ErrorOr<void> initialize(AML::Namespace& acpi_namespace);

	private:
		BatterySystem(AML::Namespace&);

		BAN::ErrorOr<void> initialize_impl();

	private:
		AML::Namespace& m_acpi_namespace;
		BAN::RefPtr<TmpDirectoryInode> m_directory;
	};

}
