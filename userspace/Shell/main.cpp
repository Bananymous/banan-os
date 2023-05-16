#include <stdio.h>
#include <termios.h>

struct termios old_termios, new_termios;

int main(int argc, char** argv)
{
	for (int i = 0; i < argc; i++)
		printf("%s\n", argv[i]);

	tcgetattr(0, &old_termios);

	new_termios = old_termios;
	new_termios.c_lflag &= ~(ECHO | ICANON);
	tcsetattr(0, TCSANOW, &new_termios);

	while (true)
	{
		char c;
		fread(&c, 1, sizeof(char), stdin);
		fputc(c, stdout);
		fflush(stdout);
	}

	tcsetattr(0, TCSANOW, &old_termios);
	return 0;
}
