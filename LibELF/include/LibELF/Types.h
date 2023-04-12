#pragma once

#include <stdint.h>

namespace LibELF
{

	using Elf64Addr = uint64_t;
	using Elf64Off = uint64_t;
	using Elf64Half = uint16_t;
	using Elf64Word = uint32_t;
	using Elf64Sword = int32_t;
	using Elf64Xword = uint64_t;
	using Elf64Sxword = int64_t;

	struct Elf64FileHeader
	{
		unsigned char e_ident[16];
		Elf64Half e_type;
		Elf64Half e_machine;
		Elf64Word e_version;
		Elf64Addr e_entry;
		Elf64Off e_phoff;
		Elf64Off e_shoff;
		Elf64Word e_flags;
		Elf64Half e_ehsize;
		Elf64Half e_phentsize;
		Elf64Half e_phnum;
		Elf64Half e_shentsize;
		Elf64Half e_shnum;
		Elf64Half e_shstrndx;
	};

	struct Elf64SectionHeader
	{
		Elf64Word sh_name;
		Elf64Word sh_type;
		Elf64Xword sh_flags;
		Elf64Addr sh_addr;
		Elf64Off sh_offset;
		Elf64Xword sh_size;
		Elf64Word sh_link;
		Elf64Word sh_info;
		Elf64Xword sh_addralign;
		Elf64Xword sh_entsize;
	};

	struct Elf64Symbol
	{
		Elf64Word st_name;
		unsigned char st_info;
		unsigned char st_other;
		Elf64Half st_shndx;
		Elf64Addr st_value;
		Elf64Xword st_size;
	};

	struct Elf64Relocation
	{
		Elf64Addr r_offset;
		Elf64Xword r_info;
	};

	struct Elf64RelocationA
	{
		Elf64Addr r_offset;
		Elf64Xword r_info;
		Elf64Sxword r_addend;
	};

	struct Elf64ProgramHeader
	{
		Elf64Word p_type;
		Elf64Word p_flags;
		Elf64Off p_offset;
		Elf64Addr p_vaddr;
		Elf64Addr p_paddr;
		Elf64Xword p_filesz;
		Elf64Xword p_memsz;
		Elf64Xword p_align;
	};

}