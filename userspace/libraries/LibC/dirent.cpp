#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/syscall.h>
#include <unistd.h>

struct __DIR
{
	int fd { -1 };
	size_t entry_count { 0 };
	size_t entry_index { 0 };
	// FIXME: we should probably allocate entries dynamically
	//        based if syscall returns ENOBUFS
	dirent entries[128];
};

int closedir(DIR* dirp)
{
	if (dirp == nullptr || dirp->fd == -1)
	{
		errno = EBADF;
		return -1;
	}

	close(dirp->fd);
	free(dirp);

	return 0;
}

int dirfd(DIR* dirp)
{
	if (dirp == nullptr || dirp->fd == -1)
	{
		errno = EINVAL;
		return -1;
	}

	return dirp->fd;
}

DIR* fdopendir(int fd)
{
	DIR* dirp = (DIR*)malloc(sizeof(DIR));
	if (dirp == nullptr)
		return nullptr;

	dirp->fd = fd;
	dirp->entry_count = 0;
	dirp->entry_index = 0;

	return dirp;
}

DIR* opendir(const char* dirname)
{
	int fd = open(dirname, O_RDONLY | O_DIRECTORY);
	if (fd == -1)
		return nullptr;
	return fdopendir(fd);
}

struct dirent* readdir(DIR* dirp)
{
	if (dirp == nullptr || dirp->fd == -1)
	{
		errno = EBADF;
		return nullptr;
	}

	dirp->entry_index++;
	if (dirp->entry_index < dirp->entry_count)
		return &dirp->entries[dirp->entry_index];

	long entry_count = syscall(SYS_READ_DIR, dirp->fd, dirp->entries, sizeof(dirp->entries) / sizeof(dirp->entries[0]));
	if (entry_count <= 0)
		return nullptr;

	dirp->entry_count = entry_count;
	dirp->entry_index = 0;

	return &dirp->entries[0];
}
