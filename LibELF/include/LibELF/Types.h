#pragma once

#include <kernel/Arch.h>

#include <stdint.h>

namespace LibELF
{

	using Elf32Addr = uint32_t;
	using Elf32Off = uint32_t;
	using Elf32Half = uint16_t;
	using Elf32Word = uint32_t;
	using Elf32Sword = int32_t;

	struct Elf32FileHeader
	{
		unsigned char e_ident[16];
		Elf32Half e_type;
		Elf32Half e_machine;
		Elf32Word e_version;
		Elf32Addr e_entry;
		Elf32Off e_phoff;
		Elf32Off e_shoff;
		Elf32Word e_flags;
		Elf32Half e_ehsize;
		Elf32Half e_phentsize;
		Elf32Half e_phnum;
		Elf32Half e_shentsize;
		Elf32Half e_shnum;
		Elf32Half e_shstrndx;
	};

	struct Elf32SectionHeader
	{
		Elf32Word sh_name;
		Elf32Word sh_type;
		Elf32Word sh_flags;
		Elf32Addr sh_addr;
		Elf32Off sh_offset;
		Elf32Word sh_size;
		Elf32Word sh_link;
		Elf32Word sh_info;
		Elf32Word sh_addralign;
		Elf32Word sh_entsize;
	};

	struct Elf32Symbol
	{
		Elf32Word st_name;
		Elf32Addr st_value;
		Elf32Word st_size;
		unsigned char st_info;
		unsigned char st_other;
		Elf32Half st_shndx;
	};

	struct Elf32Relocation
	{
		Elf32Addr r_offset;
		Elf32Word r_info;
	};

	struct Elf32RelocationA
	{
		Elf32Addr r_offset;
		Elf32Word r_info;
		Elf32Sword r_addend;
	};

	struct Elf32ProgramHeader
	{
		Elf32Word p_type;
		Elf32Off p_offset;
		Elf32Addr p_vaddr;
		Elf32Addr p_paddr;
		Elf32Word p_filesz;
		Elf32Word p_memsz;
		Elf32Word p_flags;
		Elf32Word p_align;
	};

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

#if ARCH(i386)
	using ElfNativeAddr = Elf32Addr;
	using ElfNativeOff = Elf32Off;
	using ElfNativeHalf = Elf32Half;
	using ElfNativeWord = Elf32Word;
	using ElfNativeSword = Elf32Sword;
	using ElfNativeFileHeader = Elf32FileHeader;
	using ElfNativeSectionHeader = Elf32SectionHeader;
	using ElfNativeSymbol = Elf32Symbol;
	using ElfNativeRelocation = Elf32Relocation;
	using ElfNativeRelocationA = Elf32RelocationA;
	using ElfNativeProgramHeader = Elf32ProgramHeader;
#elif ARCH(x86_64)
	using ElfNativeAddr = Elf64Addr;
	using ElfNativeOff = Elf64Off;
	using ElfNativeHalf = Elf64Half;
	using ElfNativeWord = Elf64Word;
	using ElfNativeSword = Elf64Sword;
	using ElfNativeXword = Elf64Xword;
	using ElfNativeSxword = Elf64Sxword;
	using ElfNativeFileHeader = Elf64FileHeader;
	using ElfNativeSectionHeader = Elf64SectionHeader;
	using ElfNativeSymbol = Elf64Symbol;
	using ElfNativeRelocation = Elf64Relocation;
	using ElfNativeRelocationA = Elf64RelocationA;
	using ElfNativeProgramHeader = Elf64ProgramHeader;
#endif

}
