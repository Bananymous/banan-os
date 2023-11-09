#include "ELF.h"
#include "GPT.h"

#include <iostream>

int main(int argc, char** argv)
{
	using namespace std::string_view_literals;

	if (argc != 3)
	{
		std::fprintf(stderr, "usage: %s BOOTLOADER DISK_IMAGE}\n", argv[0]);
		return 1;
	}

	ELFFile bootloader(argv[1]);
	if (!bootloader.success())
		return 1;

	auto stage1 = bootloader.find_section(".stage1"sv);
	auto stage2 = bootloader.find_section(".stage2"sv);
	if (!stage1.has_value() || !stage2.has_value())
	{
		std::cerr << bootloader.path() << " doesn't contain .stage1 and .stage2 sections" << std::endl;
		return 1;
	}

	GPTFile disk_image(argv[2]);
	if (!disk_image.success())
		return 1;

	if (!disk_image.install_bootcode(*stage1))
		return 1;
	std::cout << "wrote stage1 bootloader" << std::endl;

	if (!disk_image.write_partition(*stage2, bios_boot_guid))
		return 1;
	std::cout << "wrote stage2 bootloader" << std::endl;

	return 0;
}