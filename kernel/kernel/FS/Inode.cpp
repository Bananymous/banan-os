#include <kernel/FS/Inode.h>

#include <fcntl.h>

namespace Kernel
{

	bool Inode::can_access(const Credentials& credentials, int flags)
	{
		if (credentials.euid() == 0)
			return true;

		// We treat O_SEARCH as O_RDONLY
		if (flags & (O_RDONLY | O_SEARCH))
		{
			if (mode().mode & S_IROTH)
			{ }
			else if ((mode().mode & S_IRUSR) && credentials.euid() == uid())
			{ }
			else if ((mode().mode & S_IRGRP) && credentials.egid() == gid())
			{ }
			else
			{
				return false;
			}
		}

		if (flags & O_WRONLY)
		{
			if (mode().mode & S_IWOTH)
			{ }
			else if ((mode().mode & S_IWUSR) && credentials.euid() == uid())
			{ }
			else if ((mode().mode & S_IWGRP) && credentials.egid() == gid())
			{ }
			else
			{
				return false;
			}
		}

		if (flags & O_EXEC)
		{
			if (mode().mode & S_IXOTH)
			{ }
			else if ((mode().mode & S_IXUSR) && credentials.euid() == uid())
			{ }
			else if ((mode().mode & S_IXGRP) && credentials.egid() == gid())
			{ }
			else
			{
				return false;
			}
		}

		return true;
	}

}