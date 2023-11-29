#include "Image.h"

#include <stdio.h>
#include <unistd.h>

int usage(char* arg0, int ret)
{
	FILE* out = (ret == 0) ? stdout : stderr;
	fprintf(out, "usage: %s IMAGE_PATH\n", arg0);
	return ret;
}

int main(int argc, char** argv)
{
	if (argc != 2)
		return usage(argv[0], 1);
	
	auto image = Image::load_from_file(argv[1]);
	if (!image)
		return 1;

	if (!image->render_to_framebuffer())
		return 1;

	for (;;)
		sleep(1);

	return 0;
}
