#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

int main(int argc, char** argv)
{
	if (argc != 3)
	{
		fprintf(stderr, "usage: %s MOUNT_POINT PERMS_FILE\n", argv[0]);
		return 1;
	}

	int mount_fd = open(argv[1], O_RDONLY | O_DIRECTORY);
	if (mount_fd == -1)
	{
		fprintf(stderr, "could not open %s: %s\n", argv[1], strerror(errno));
		return 1;
	}

	FILE* perm_fp = fopen(argv[2], "r");
	if (perm_fp == NULL)
	{
		fprintf(stderr, "could not open %s: %s\n", argv[2], strerror(errno));
		return 1;
	}

	char buffer[1024];
	while (fgets(buffer, sizeof(buffer), perm_fp))
	{
		char path[PATH_MAX];
		uid_t uid;
		gid_t gid;
		mode_t mode;

		if (sscanf(buffer, "%[^|\n]|%d|%d|%o\n", path, &uid, &gid, &mode) != 4)
			continue;

		struct stat st;
		if (fstatat(mount_fd, path, &st, AT_SYMLINK_NOFOLLOW) != 0)
			continue;

		if (st.st_uid != uid || st.st_gid != gid)
			if (fchownat(mount_fd, path, uid, gid, AT_SYMLINK_NOFOLLOW) != 0)
				fprintf(stderr, "fchownat: %s: %s\n", path, strerror(errno));

		const mode_t mode_mask = S_ISUID | S_ISGID | S_ISVTX | 0777;
		if (mode != (st.st_mode & mode_mask))
			if (fchmodat(mount_fd, path, mode, AT_SYMLINK_NOFOLLOW) != 0)
				fprintf(stderr, "fchmodat: %s: %s\n", path, strerror(errno));
	}

	fclose(perm_fp);
	close(mount_fd);

	return 0;
}
