#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/syscall.h>
#include <unistd.h>

#include <kernel/API/DirectoryEntry.h>

struct __DIR
{
	int fd { -1 };
	size_t entry_index { 0 };
	Kernel::API::DirectoryEntry* current { nullptr };

	size_t buffer_size { 0 };
	Kernel::API::DirectoryEntryList buffer[];
};

int closedir(DIR* dirp)
{
	if (dirp == nullptr || dirp->fd == -1)
	{
		errno = EBADF;
		return -1;
	}
	
	close(dirp->fd);
	dirp->fd = -1;
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
	DIR* dirp = (DIR*)malloc(sizeof(DIR) + 4096);
	if (dirp == nullptr)
		return nullptr;

	dirp->fd = fd;
	dirp->current = nullptr;
	dirp->buffer_size = 4096;

	return dirp;
}

DIR* opendir(const char* dirname)
{
	int fd = open(dirname, O_RDONLY);
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
	if (dirp->current && dirp->entry_index < dirp->buffer->entry_count)
	{
		dirp->current = dirp->current->next();
		return &dirp->current->dirent;
	}

	if (syscall(SYS_READ_DIR_ENTRIES, dirp->fd, dirp->buffer, dirp->buffer_size) == -1)
		return nullptr;

	if (dirp->buffer->entry_count == 0)
		return nullptr;

	dirp->entry_index = 0;
	dirp->current = dirp->buffer->array;

	return &dirp->current->dirent;
}
