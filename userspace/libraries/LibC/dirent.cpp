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
	size_t entries_size { 0 };
	dirent* entries { nullptr };

	static constexpr size_t default_entries_size { 128 };
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
	DIR* dirp = static_cast<DIR*>(malloc(sizeof(DIR)));
	if (dirp == nullptr)
		return nullptr;

	dirp->entries = static_cast<dirent*>(malloc(DIR::default_entries_size * sizeof(dirent)));
	dirp->entries_size = DIR::default_entries_size;
	if (dirp->entries == nullptr)
	{
		free(dirp);
		return nullptr;
	}

	dirp->fd = fd;
	dirp->entry_count = 0;
	dirp->entry_index = 0;

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
	if (dirp->entry_index < dirp->entry_count)
		return &dirp->entries[dirp->entry_index];

readdir_do_syscall:
	long entry_count = syscall(SYS_READ_DIR, dirp->fd, dirp->entries, dirp->entries_size);
	if (entry_count == -1 && errno == ENOBUFS)
	{
		const size_t new_entries_size = dirp->entries_size * 2;
		dirent* new_entries = static_cast<dirent*>(malloc(new_entries_size * sizeof(dirent)));
		if (new_entries == nullptr)
			return nullptr;
		dirp->entries = new_entries;
		dirp->entries_size = new_entries_size;
		goto readdir_do_syscall;
	}
	if (entry_count <= 0)
		return nullptr;

	dirp->entry_count = entry_count;
	dirp->entry_index = 0;

	return &dirp->entries[0];
}
