#include <stdio.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>

#include <BAN/Time.h>

void print_timestamp(timespec ts)
{
	auto time = BAN::from_unix_time(ts.tv_sec);
	printf("%04d-%02d-%02d %02d:%02d:%02d.%09d",
		time.year, time.month, time.day,
		time.hour, time.minute, time.second,
		ts.tv_nsec
	);
}

int main(int argc, char** argv)
{
	for (int i = 1; i < argc; i++)
	{
		struct stat st;
		if (stat(argv[i], &st) == -1)
		{
			perror("stat");
			continue;
		}

		char access[11];
		const char* type = nullptr;
		if (S_ISBLK(st.st_mode)) {
			access[0] = 'b';
			type = "block special file";
		} else if (S_ISCHR(st.st_mode)) {
			access[0] = 'c';
			type = "character special file";
		} else if (S_ISDIR(st.st_mode)) {
			access[0] = 'd';
			type = "directory";
		} else if (S_ISFIFO(st.st_mode)) {
			access[0] = 'f';
			type = "fifo";
		} else if (S_ISREG(st.st_mode)) {
			access[0] = '-';
			type = "regular file";
		} else if (S_ISLNK(st.st_mode)) {
			access[0] = 'l';
			type = "symbolic link";
		} else if (S_ISSOCK(st.st_mode)) {
			access[0] = 's';
			type = "socket";
		} else {
			access[0] = '-';
			type = "unknown";
		}

		access[1] = (st.st_mode & S_IRUSR) ? 'r' : '-';
		access[2] = (st.st_mode & S_IWUSR) ? 'w' : '-';
		access[3] = (st.st_mode & S_IXUSR) ? 'x' : '-';
		access[4] = (st.st_mode & S_IRGRP) ? 'r' : '-';
		access[5] = (st.st_mode & S_IWGRP) ? 'w' : '-';
		access[6] = (st.st_mode & S_IXGRP) ? 'x' : '-';
		access[7] = (st.st_mode & S_IROTH) ? 'r' : '-';
		access[8] = (st.st_mode & S_IWOTH) ? 'w' : '-';
		access[9] = (st.st_mode & S_IXOTH) ? 'x' : '-';
		access[10] = '\0';

		printf("  File: %s\n", argv[i]);
		printf("  Size: %-15d Blocks: %-10d IO Block: %-6d %s\n", (int)st.st_size, (int)st.st_blocks, (int)st.st_blksize, type);
		printf("Device: %d,%-5d Inode: %-11d Links: %-5d Device type: %d,%d\n", (int)major(st.st_dev), (int)minor(st.st_dev), (int)st.st_ino, (int)st.st_nlink, (int)major(st.st_rdev), (int)minor(st.st_rdev));
		printf("Access: (%04o/%s)  Uid: %5d Gid: %5d\n", (int)(st.st_mode & S_IRWXMASK), access, (int)st.st_uid, (int)st.st_gid);
		printf("Access: "); print_timestamp(st.st_atim); printf("\n");
		printf("Modify: "); print_timestamp(st.st_mtim); printf("\n");
		printf("Change: "); print_timestamp(st.st_ctim); printf("\n");
	}
}
