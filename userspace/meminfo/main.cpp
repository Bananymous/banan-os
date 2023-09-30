#include <ctype.h>
#include <dirent.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/banan-os.h>

bool is_only_digits(const char* str)
{
	while (*str)
		if (!isdigit(*str++))
			return false;
	return true;
}

int main()
{
	DIR* proc = opendir("/proc");
	if (proc == nullptr)
	{
		perror("opendir");
		return 1;
	}

	char path_buffer[128] {};
	while (dirent* proc_ent = readdir(proc))
	{
		if (proc_ent->d_type != DT_DIR)
			continue;
		if (!is_only_digits(proc_ent->d_name))
			continue;

		strcpy(path_buffer, proc_ent->d_name);
		strcat(path_buffer, "/meminfo");
	
		int fd = openat(dirfd(proc), path_buffer, O_RDONLY);
		if (fd == -1)
		{
			perror("openat");
			continue;
		}

		proc_meminfo_t meminfo;
		if (read(fd, &meminfo, sizeof(meminfo)) == -1)
			perror("read");
		else
		{
			printf("process:\n");
			printf("  pid:  %s\n", proc_ent->d_name);
			printf("  vmem: %zu pages (%zu bytes)\n", meminfo.virt_pages, meminfo.page_size * meminfo.virt_pages);
		}

		close(fd);
	}

	closedir(proc);
}
