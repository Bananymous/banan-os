#pragma once

#include <dirent.h>

namespace Kernel::API
{

	struct DirectoryEntry
	{
		size_t rec_len { 0 };
		struct dirent dirent;
		DirectoryEntry* next() const { return (DirectoryEntry*)((uintptr_t)this + rec_len); }
	};

	struct DirectoryEntryList
	{
		size_t entry_count { 0 };
		DirectoryEntry array[];
	};

}