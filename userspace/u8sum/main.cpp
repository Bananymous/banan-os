#include <fcntl.h>
#include <stdio.h>

int main(int argc, char** argv)
{
	for (int i = 1; i < argc; i++)
	{
		FILE* fp = fopen(argv[i], "r");
		if (fp == nullptr)
		{
			perror("fopen");
			continue;
		}

		uint8_t sum = 0;

		uint8_t buffer[1024];
		while (size_t ret = fread(buffer, 1, sizeof(buffer), fp))
			for (int j = 0; j < ret; j++)
				sum += buffer[j];

		if (ferror(fp))
			perror("fread");
		else
			printf("%s: %02x\n", argv[i], sum);

		fclose(fp);
	}
}
