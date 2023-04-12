#pragma once

namespace LibELF
{

	enum ELF_Ident
	{
		ELFMAG0			= 0x7F,
		ELFMAG1			= 'E',
		ELFMAG2			= 'L',
		ELFMAG3			= 'F',

		ELFCLASSNONE	= 0,
		ELFCLASS32		= 1,
		ELFCLASS64		= 2,

		ELFDATANONE		= 0,
		ELFDATA2LSB		= 1,
		ELFDATA2MSB		= 2,
	};

	enum ELF_EI
	{
		EI_MAG0			= 0,
		EI_MAG1			= 1,
		EI_MAG2			= 2,
		EI_MAG3			= 3,
		EI_CLASS		= 4,
		EI_DATA			= 5,
		EI_VERSION		= 6,
		EI_OSABI		= 7,
		EI_ABIVERSION	= 8,
		EI_NIDENT		= 16,
	};

	enum ELF_ET
	{
		ET_NONE		= 0,
		ET_REL		= 1,
		ET_EXEC		= 2,
		ET_DYN		= 3,
		ET_CORE		= 4,
		ET_LOOS		= 0xfe00,
		ET_HIOS		= 0xfeff,
		ET_LOPROC	= 0xff00,
		ET_HIPROC	= 0xffff,
	};

	enum ELF_EV
	{
		EV_NONE		= 0,
		EV_CURRENT	= 1,
	};

	enum ELF_SHT
	{
		SHT_NULL		= 0,
		SHT_PROGBITS	= 1,
		SHT_SYMTAB		= 2,
		SHT_STRTAB		= 3,
		SHT_RELA		= 4,
		SHT_NOBITS		= 8,
		SHT_REL			= 9,
		SHT_SHLIB		= 10,
		SHT_DYNSYM		= 11,
		SHT_LOOS		= 0x60000000,
		SHT_HIOS		= 0x6FFFFFFF,
		SHT_LOPROC		= 0x70000000,
		SHT_HIPROC		= 0x7FFFFFFF,
	};

	enum ELF_SHF
	{
		SHF_WRITE		= 0x1,
		SHF_ALLOC		= 0x2,
		SHF_EXECINSTR	= 0x4,
		SHF_MASKOS		= 0x0F000000,
		SHF_MASKPROC	= 0xF0000000,
	};

	enum ELF_SHN
	{
		SHN_UNDEF	= 0,
		SHN_LOPROC	= 0xFF00,
		SHN_HIPROC	= 0xFF1F,
		SHN_LOOS	= 0xFF20,
		SHN_HIOS	= 0xFF3F,
		SHN_ABS		= 0xFFF1,
		SHN_COMMON	= 0xFFF2,
	};

	enum ELF_STB
	{
		STB_LOCAL	= 0,
		STB_GLOBAL	= 1,
		STB_WEAK	= 2,
		STB_LOOS	= 10,
		STB_HIOS	= 12,
		STB_LOPROC	= 13,
		STB_HIPROC	= 15,
	};

	enum ELF_STT
	{
		STT_NOTYPE	= 0,
		STT_OBJECT	= 1,
		STT_FUNC	= 2,
		STT_SECTION	= 3,
		STT_FILE	= 4,
		STT_LOOS	= 10,
		STT_HIOS	= 12,
		STT_LOPROC	= 13,
		STT_HIPROC	= 15,
	};

	enum ELF_PT
	{
		PT_NULL		= 0,
		PT_LOAD		= 1,
		PT_DYNAMIC	= 2,
		PT_INTERP	= 3,
		PT_NOTE		= 4,
		PT_SHLIB	= 5,
		PT_PHDR		= 6,
		PT_LOOS		= 0x60000000,
		PT_HIOS		= 0x6FFFFFFF,
		PT_LOPROC	= 0x70000000,
		PT_HIPROC	= 0x7FFFFFFF,
	};

	enum ELF_PF
	{
		PF_X		= 0x1,
		PF_W		= 0x2,
		PF_R		= 0x4,
		PF_MASKOS	= 0x00FF0000,
		PF_MASKPROC	= 0xFF000000,
	};

}