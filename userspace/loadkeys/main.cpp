#include <stdio.h>
#include <string.h>
#include <sys/banan-os.h>
#include <sys/stat.h>

int try_load_keymap(const char* path)
{
	if (load_keymap(path) == -1)
	{
		perror("load_keymap");
		return 1;
	}
	return 0;
}

int main(int argc, char** argv)
{
	if (argc != 2)
	{
		fprintf(stderr, "usage: %s KEYMAP\n", argv[0]);
		return 1;
	}

	struct stat st;
	if (stat(argv[1], &st) == 0)
		return try_load_keymap(argv[1]);

	char buffer[128];
	strcpy(buffer, "/usr/share/keymaps/");
	strcat(buffer, argv[1]);

	if (stat(buffer, &st) == 0)
		return try_load_keymap(buffer);

	strcat(buffer, ".keymap");

	if (stat(buffer, &st) == 0)
		return try_load_keymap(buffer);

	fprintf(stderr, "Keymap '%s' not found\n", argv[1]);
	return 1;
}
