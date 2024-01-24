#include <ctype.h>
#include <dirent.h>
#include <errno.h>
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

		{
			strcpy(path_buffer, proc_ent->d_name);
			strcat(path_buffer, "/cmdline");

			int fd = openat(dirfd(proc), path_buffer, O_RDONLY);
			if (fd == -1)
			{
				if (errno != EACCES)
					perror("openat");
				continue;
			}

			printf("process: ");

			while (ssize_t nread = read(fd, path_buffer, sizeof(path_buffer) - 1))
			{
				if (nread < 0)
				{
					perror("read");
					break;
				}
				for (int i = 0; i < nread; i++)
					if (path_buffer[i] == '\0')
						path_buffer[i] = ' ';

				path_buffer[nread] = '\0';

				int written = 0;
				while (written < nread)
					written += printf("%s ", path_buffer + written);
			}

			printf("\n");

			close(fd);
		}

		printf("  pid:  %s\n", proc_ent->d_name);

		{
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
				size_t percent_times_100 = 10000 * meminfo.phys_pages / meminfo.virt_pages;
				printf("  vmem: %zu pages (%zu bytes)\n", meminfo.virt_pages, meminfo.page_size * meminfo.virt_pages);
				printf("  pmem: %zu pages (%zu bytes) %zu.%02zu%%\n", meminfo.phys_pages, meminfo.page_size * meminfo.phys_pages, percent_times_100 / 100, percent_times_100 % 100);
			}

			close(fd);
		}
	}

	closedir(proc);
}
