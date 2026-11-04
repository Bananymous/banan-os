#include "ELF.h"
#include "GPT.h"

#include <iostream>

int main(int argc, char** argv)
{
	using namespace std::string_view_literals;

	if (argc != 4)
	{
		std::fprintf(stderr, "usage: %s BOOTLOADER DISK_IMAGE ROOT_PARTITION_NAME\n", argv[0]);
		return 1;
	}

	ELFFile bootloader(argv[1]);
	if (!bootloader.success())
		return 1;

	auto stage1 = bootloader.find_section(".stage1"sv);
	auto stage2 = bootloader.find_section(".stage2"sv);
	auto data = bootloader.find_section(".data"sv);
	if (!stage1.has_value() || !stage2.has_value() || !data.has_value())
	{
		std::cerr << bootloader.path() << " doesn't contain .stage1, .stage2 and .data sections" << std::endl;
		return 1;
	}

	GPTFile disk_image(argv[2]);
	if (!disk_image.success())
		return 1;

	if (!disk_image.install_bootloader(*stage1, *stage2, *data, argv[3]))
		return 1;
	std::cout << "bootloader installed" << std::endl;

	return 0;
}
