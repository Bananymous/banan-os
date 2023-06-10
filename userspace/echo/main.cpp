#include <stdio.h>
#include <stdlib.h>

void print_argument(const char* arg)
{
	while (*arg)
	{
		if (*arg == '\\')
		{
			switch (*(++arg))
			{
				case 'a': fputc('\a', stdout); break;
				case 'b': fputc('\b', stdout); break;
				case 'c': exit(0);
				case 'f': fputc('\f', stdout); break;
				case 'n': fputc('\n', stdout); break;
				case 'r': fputc('\r', stdout); break;
				case 't': fputc('\t', stdout); break;
				case 'v': fputc('\v', stdout); break;
				case '\\': fputc('\\', stdout); break;
				default: break;
			}
		}
		else
			fputc(*arg, stdout);
		arg++;
	}
}

int main(int argc, char** argv)
{
	for (int i = 1; i < argc; i++)
	{
		print_argument(argv[i]);
		if (i < argc - 1)
			fputc(' ', stdout);
	}
	printf("\n");
	return 0;
}
