#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
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
	free(dirp->entries);
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
	static dirent s_dirent;
	dirent* result;

	if (int error = readdir_r(dirp, &s_dirent, &result))
	{
		errno = error;
		return nullptr;
	}

	return result;
}

int readdir_r(DIR* dirp, struct dirent* __restrict entry, struct dirent** __restrict result)
{
	if (dirp == nullptr || dirp->fd == -1)
		return EBADF;

	dirp->entry_index++;
	if (dirp->entry_index < dirp->entry_count)
	{
		*entry = dirp->entries[dirp->entry_index];
		*result = entry;
		return 0;
	}

readdir_do_syscall:
	long entry_count = syscall(SYS_READ_DIR, dirp->fd, dirp->entries, dirp->entries_size);
	if (entry_count == -1 && errno == ENOBUFS)
	{
		const size_t new_entries_size = dirp->entries_size * 2;
		dirent* new_entries = static_cast<dirent*>(malloc(new_entries_size * sizeof(dirent)));
		if (new_entries == nullptr)
			return errno;
		free(dirp->entries);
		dirp->entries = new_entries;
		dirp->entries_size = new_entries_size;
		goto readdir_do_syscall;
	}

	*result = nullptr;
	if (entry_count < 0)
		return errno;
	if (entry_count == 0)
		return 0;

	dirp->entry_count = entry_count;
	dirp->entry_index = 0;

	*entry = dirp->entries[0];
	*result = entry;
	return 0;
}

void rewinddir(DIR* dirp)
{
	dirp->entry_count = 0;
	dirp->entry_index = 0;
	lseek(dirp->fd, 0, SEEK_SET);
}

int alphasort(const struct dirent** d1, const struct dirent** d2)
{
	return strcoll((*d1)->d_name, (*d2)->d_name);
}

int scandir(const char* dir, struct dirent*** namelist, int (*sel)(const struct dirent*), int (*compar)(const struct dirent**, const struct dirent**))
{
	DIR* dirp = opendir(dir);
	if (dirp == nullptr)
		return -1;

	size_t count = 0;
	dirent** list = nullptr;

	dirent* dent;
	while ((dent = readdir(dirp)))
	{
		if (sel && sel(dent) == 0)
			continue;

		void* new_list = realloc(list, (count + 1) * sizeof(dirent*));
		if (new_list == nullptr)
			goto scandir_error;

		list = static_cast<dirent**>(new_list);
		list[count] = static_cast<dirent*>(malloc(sizeof(dirent)));
		if (list[count] == nullptr)
			goto scandir_error;

		memcpy(list[count], dent, sizeof(dirent));
		count++;
	}

	closedir(dirp);

	qsort(list, count, sizeof(dirent*), reinterpret_cast<int(*)(const void*, const void*)>(compar));

	*namelist = list;
	return count;

scandir_error:
	closedir(dirp);

	for (size_t i = 0; i < count; i++)
		free(list[i]);
	free(list);

	*namelist = nullptr;
	return -1;
}
