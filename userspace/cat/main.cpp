#include <stdio.h>

bool cat_file(FILE* fp)
{
	char buffer[1025];
	size_t n_read;
	while ((n_read = fread(buffer, 1, sizeof(buffer) - 1, fp)) > 0)
	{
		buffer[n_read] = '\0';
		fputs(buffer, stdout);
	}
	if (ferror(fp))
	{
		perror("fread");
		return false;
	}
	return true;
}

int main(int argc, char** argv)
{
	int ret = 0;

	if (argc > 1)
	{
		for (int i = 1; i < argc; i++)
		{
			FILE* fp = fopen(argv[i], "r");
			if (fp == nullptr)
			{
				perror(argv[i]);
				ret = 1;
				continue;
			}
			if (!cat_file(fp))
				ret = 1;
			fclose(fp);
		}
	}
	else
	{
		if (!cat_file(stdin))
			ret = 1;
	}

	return ret;
}
