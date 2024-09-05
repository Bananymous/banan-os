#include "utils.h"

#include <LibELF/Types.h>
#include <LibELF/Values.h>

#include <fcntl.h>
#include <limits.h>
#include <sys/mman.h>
#include <sys/syscall.h>
#include <unistd.h>

extern "C"
__attribute__((naked))
void _start()
{
#if defined(__x86_64__)
	asm volatile(
		"xorq %rbp, %rbp;"
		"call _entry;"
		"ud2;"
	);
#elif defined(__i686__)
	asm volatile(
		"xorl %ebp, %ebp;"
		"pushl %ecx;"
		"pushl %edx;"
		"pushl %esi;"
		"pushl %edi;"
		"call _entry;"
		"ud2;"
	);
#else
	#error "unsupported architecture"
#endif
}

__attribute__((naked, noreturn))
static void call_entry_point(int, char**, char**, uintptr_t)
{
#if defined(__x86_64__)
	asm volatile(
		"andq $-16, %rsp;"
		"jmp *%rcx;"
	);
#elif defined(__i686__)
	asm volatile(
		"addl $4, %esp;"
		"popl %edi;"
		"popl %esi;"
		"popl %edx;"
		"popl %ecx;"
		"andl $-16, %esp;"
		"jmp *%ecx;"
	);
#else
	#error "unsupported architecture"
#endif
}

using namespace LibELF;

static void validate_program_header(const ElfNativeFileHeader& file_header)
{
	if (file_header.e_ident[EI_MAG0] != ELFMAG0 ||
		file_header.e_ident[EI_MAG1] != ELFMAG1 ||
		file_header.e_ident[EI_MAG2] != ELFMAG2 ||
		file_header.e_ident[EI_MAG3] != ELFMAG3)
	{
		print_error_and_exit("ELF has invalid magic in header", 0);
	}

	if (file_header.e_ident[EI_DATA] != ELFDATA2LSB)
		print_error_and_exit("ELF is not little-endian", 0);

	if (file_header.e_ident[EI_VERSION] != EV_CURRENT)
		print_error_and_exit("ELF has invalid version", 0);

#if defined(__x86_64__)
	if (file_header.e_ident[EI_CLASS] != ELFCLASS64)
#elif defined(__i686__)
	if (file_header.e_ident[EI_CLASS] != ELFCLASS32)
#else
	#error "unsupported architecture"
#endif
		print_error_and_exit("ELF not in native format", 0);

	if (file_header.e_type != ET_EXEC && file_header.e_type != ET_DYN)
		print_error_and_exit("ELF has unsupported file header type", 0);

	if (file_header.e_version != EV_CURRENT)
		print_error_and_exit("ELF has unsupported version", 0);
}

__attribute__((naked))
static void resolve_symbol_trampoline()
{
#if defined(__x86_64__)
	asm volatile(
		"pushq %rdi;"
		"pushq %rsi;"
		"pushq %rdx;"
		"pushq %rcx;"
		"pushq %r8;"
		"pushq %r9;"
		"pushq %r10;"
		"pushq %r11;"

		"movq 64(%rsp), %rdi;"
		"movq 72(%rsp), %rsi;"

		"call resolve_symbol;"

		"popq %r11;"
		"popq %r10;"
		"popq %r9;"
		"popq %r8;"
		"popq %rcx;"
		"popq %rdx;"
		"popq %rsi;"
		"popq %rdi;"

		"addq $16, %rsp;"
		"jmp *%rax;"
	);
#elif defined(__i686__)
	asm volatile(
		"call resolve_symbol;"
		"addl $8, %esp;"
		"jmp *%eax;"
	);
#else
	#error "unsupported architecture"
#endif
}

struct LoadedElf
{
	ElfNativeFileHeader file_header;
	ElfNativeDynamic* dynamics;

	uintptr_t base;

	uintptr_t hash;

	uintptr_t strtab;

	uintptr_t symtab;
	size_t syment;

	uintptr_t rel;
	size_t relent;
	size_t relsz;

	uintptr_t rela;
	size_t relaent;
	size_t relasz;

	uintptr_t jmprel;
	size_t pltrel;
	size_t pltrelsz;

	uintptr_t init;
	uintptr_t init_array;
	size_t init_arraysz;

	bool has_called_init;
	bool is_relocated;

	char path[PATH_MAX];
};

constexpr uintptr_t SYM_NOT_FOUND = -1;

static uint32_t elf_hash(const char* name)
{
	uint32_t h = 0, g;
	while (*name)
	{
		h = (h << 4) + *name++;
		if ((g = h & 0xF0000000))
			h ^= g >> 24;
		h &= ~g;
	}
	return h;
}

static ElfNativeSymbol* find_symbol(const LoadedElf& elf, const char* name)
{
	const uint32_t* hash_table = reinterpret_cast<uint32_t*>(elf.hash);
	const uint32_t nbucket = hash_table[0];

	for (uint32_t entry = hash_table[2 + (elf_hash(name) % nbucket)]; entry; entry = hash_table[2 + nbucket + entry])
	{
		auto& symbol = *reinterpret_cast<ElfNativeSymbol*>(elf.symtab + entry * elf.syment);
		if (symbol.st_shndx == 0)
			continue;
		const char* symbol_name = reinterpret_cast<const char*>(elf.strtab + symbol.st_name);
		if (strcmp(name, symbol_name))
			continue;
		return &symbol;
	}

	return nullptr;
}

static uintptr_t get_symbol_address(const LoadedElf& elf, const char* name)
{
	auto* symbol = find_symbol(elf, name);
	if (symbol == nullptr)
		return SYM_NOT_FOUND;
	return elf.base + symbol->st_value;
}

static LoadedElf* get_libc_elf();
static LoadedElf* get_libgcc_elf();

template<typename RelocT> requires BAN::is_same_v<RelocT, ElfNativeRelocation> || BAN::is_same_v<RelocT, ElfNativeRelocationA>
static uintptr_t handle_relocation(const LoadedElf& elf, const RelocT& reloc)
{
	uintptr_t symbol_address = 0;
	size_t symbol_size = 0;

#if defined(__x86_64__)
	const bool is_copy = (ELF64_R_TYPE(reloc.r_info) == R_X86_64_COPY);
	if (const uint32_t symbol_index = ELF64_R_SYM(reloc.r_info))
#elif defined(__i686__)
	const bool is_copy = (ELF32_R_TYPE(reloc.r_info) == R_386_COPY);
	if (const uint32_t symbol_index = ELF32_R_SYM(reloc.r_info))
#else
	#error "unsupported architecture"
#endif
	{
		const auto& symbol = *reinterpret_cast<ElfNativeSymbol*>(elf.symtab + symbol_index * elf.syment);
		const char* symbol_name = reinterpret_cast<const char*>(elf.strtab + symbol.st_name);

		symbol_size = symbol.st_size;

		if (!is_copy && symbol.st_shndx)
			symbol_address = elf.base + symbol.st_value;
		else
		{
			// external symbol
			symbol_address = SYM_NOT_FOUND;
			for (size_t i = 0; symbol_address == SYM_NOT_FOUND; i++)
			{
				auto& dynamic = elf.dynamics[i];
				if (dynamic.d_tag == DT_NULL)
					break;
				if (dynamic.d_tag != DT_NEEDED)
					continue;
				const auto& lib_elf = *reinterpret_cast<LoadedElf*>(dynamic.d_un.d_ptr);
				symbol_address = get_symbol_address(lib_elf, symbol_name);
			}

			// libgcc_s.so needs symbols from libc, but we can't link it as toolchain
			// has to be built before libc. This hack allows resolving symbols from
			// libc even if its not specified as dependency, but is loaded
			if (symbol_address == SYM_NOT_FOUND)
				if (const auto* libc_elf = get_libc_elf())
					symbol_address = get_symbol_address(*libc_elf, symbol_name);
			if (symbol_address == SYM_NOT_FOUND)
				if (const auto* libgcc_elf = get_libgcc_elf())
					symbol_address = get_symbol_address(*libgcc_elf, symbol_name);

			if (symbol_address == SYM_NOT_FOUND)
			{
				if (ELF_ST_BIND(symbol.st_info) != STB_WEAK)
				{
					print(STDERR_FILENO, elf.path);
					print(STDERR_FILENO, ": could not find symbol \"");
					print(STDERR_FILENO, symbol_name);
					print_error_and_exit("\"", 0);
				}
				symbol_address = 0;
			}
		}
	}

	size_t size = 0;
	uintptr_t value = 0;
	bool add_addend = false;

#if defined(__x86_64__)
	switch (ELF64_R_TYPE(reloc.r_info))
	{
		case R_X86_64_NONE:
			break;
		case R_X86_64_64:
			size = 8;
			value = symbol_address;
			add_addend = true;
			break;
		case R_X86_64_COPY:
			if (symbol_address == 0)
				print_error_and_exit("copy undefined weak symbol?", 0);
			memcpy(
				reinterpret_cast<void*>(elf.base + reloc.r_offset),
				reinterpret_cast<void*>(symbol_address),
				symbol_size
			);
			break;
		case R_X86_64_GLOB_DAT:
			size = 8;
			value = symbol_address;
			break;
		case R_X86_64_JUMP_SLOT:
			size = 8;
			value = symbol_address;
			break;
		case R_X86_64_RELATIVE:
			size = 8;
			value = elf.base;
			add_addend = true;
			break;
		default:
			print(STDERR_FILENO, "unsupported reloc type ");
			print_uint(STDERR_FILENO, ELF64_R_TYPE(reloc.r_info));
			print(STDERR_FILENO, " in ");
			print(STDERR_FILENO, elf.path);
			print_error_and_exit("", 0);
	}
#elif defined(__i686__)
	switch (ELF32_R_TYPE(reloc.r_info))
	{
		case R_386_NONE:
			break;
		case R_386_32:
			size = 4;
			value = symbol_address;
			add_addend = true;
			break;
		case R_386_PC32:
			size = 4;
			value = symbol_address - reloc.r_offset;
			add_addend = true;
			break;
		case R_386_COPY:
			memcpy(
				reinterpret_cast<void*>(elf.base + reloc.r_offset),
				reinterpret_cast<void*>(symbol_address),
				symbol_size
			);
			break;
		case R_386_GLOB_DAT:
			size = 4;
			value = symbol_address;
			break;
		case R_386_JMP_SLOT:
			size = 4;
			value = symbol_address;
			break;
		case R_386_RELATIVE:
			size = 4;
			value = elf.base;
			add_addend = true;
			break;
		default:
			print(STDERR_FILENO, "unsupported reloc type ");
			print_uint(STDERR_FILENO, ELF32_R_TYPE(reloc.r_info));
			print_error_and_exit("", 0);
	}
#else
	#error "unsupported architecture"
#endif

	if (add_addend)
	{
		if constexpr(BAN::is_same_v<RelocT, ElfNativeRelocationA>)
			value += reloc.r_addend;
		else
		{
			switch (size)
			{
				case 0: break;
				case 1: value += *reinterpret_cast<uint8_t*> (elf.base + reloc.r_offset); break;
				case 2: value += *reinterpret_cast<uint16_t*>(elf.base + reloc.r_offset); break;
				case 4: value += *reinterpret_cast<uint32_t*>(elf.base + reloc.r_offset); break;
				case 8: value += *reinterpret_cast<uint64_t*>(elf.base + reloc.r_offset); break;
			}
		}
	}

	switch (size)
	{
		case 0: break;
		case 1: *reinterpret_cast<uint8_t*> (elf.base + reloc.r_offset) = value; break;
		case 2: *reinterpret_cast<uint16_t*>(elf.base + reloc.r_offset) = value; break;
		case 4: *reinterpret_cast<uint32_t*>(elf.base + reloc.r_offset) = value; break;
		case 8: *reinterpret_cast<uint64_t*>(elf.base + reloc.r_offset) = value; break;
	}

	return value;
}

static void relocate_elf(LoadedElf& elf, bool lazy_load)
{
	if (elf.is_relocated)
		return;

	// relocate libraries
	for (size_t i = 0;; i++)
	{
		auto& dynamic = elf.dynamics[i];
		if (dynamic.d_tag == DT_NULL)
			break;
		if (dynamic.d_tag != DT_NEEDED)
			continue;
		relocate_elf(*reinterpret_cast<LoadedElf*>(dynamic.d_un.d_ptr), lazy_load);
	}

	if (elf.is_relocated)
		return;

	// do "normal" relocations
	if (elf.rel && elf.relent)
		for (size_t i = 0; i < elf.relsz / elf.relent; i++)
			handle_relocation(elf, *reinterpret_cast<ElfNativeRelocation*>(elf.rel + i * elf.relent));
	if (elf.rela && elf.relaent)
		for (size_t i = 0; i < elf.relasz / elf.relaent; i++)
			handle_relocation(elf, *reinterpret_cast<ElfNativeRelocationA*>(elf.rela + i * elf.relaent));

	// do jumprel relocations
	if (elf.jmprel && elf.pltrelsz)
	{
		if (elf.pltrel != DT_REL && elf.pltrel != DT_RELA)
			print_error_and_exit("invalid value for DT_PLTREL", 0);

		if (!lazy_load)
		{
			switch (elf.pltrel)
			{
				case DT_REL:
					for (size_t i = 0; i < elf.pltrelsz / sizeof(ElfNativeRelocation); i++)
						handle_relocation(elf, reinterpret_cast<ElfNativeRelocation*>(elf.jmprel)[i]);
					break;
				case DT_RELA:
					for (size_t i = 0; i < elf.pltrelsz / sizeof(ElfNativeRelocationA); i++)
						handle_relocation(elf, reinterpret_cast<ElfNativeRelocationA*>(elf.jmprel)[i]);
					break;
			}
		}
		else
		{
			const size_t pltrelent = (elf.pltrel == DT_REL)
				? sizeof(ElfNativeRelocation)
				: sizeof(ElfNativeRelocationA);

			for (size_t i = 0; i < elf.pltrelsz / pltrelent; i++)
			{
				const auto info = (elf.pltrel == DT_REL)
					? reinterpret_cast<ElfNativeRelocation*>(elf.jmprel)[i].r_info
					: reinterpret_cast<ElfNativeRelocationA*>(elf.jmprel)[i].r_info;
				const auto offset = (elf.pltrel == DT_REL)
					? reinterpret_cast<ElfNativeRelocation*>(elf.jmprel)[i].r_offset
					: reinterpret_cast<ElfNativeRelocationA*>(elf.jmprel)[i].r_offset;

#if defined(__x86_64__)
				if (ELF64_R_TYPE(info) != R_X86_64_JUMP_SLOT)
					print_error_and_exit("jmprel relocation not R_X86_64_JUMP_SLOT", 0);
#elif defined(__i686__)
				if (ELF32_R_TYPE(info) != R_386_JMP_SLOT)
					print_error_and_exit("jmprel relocation not R_386_JMP_SLOT", 0);
#else
				#error "unsupported architecture"
#endif

				*reinterpret_cast<uintptr_t*>(elf.base + offset) += elf.base;
			}
		}
	}

	elf.is_relocated = true;
}

extern "C"
__attribute__((used))
uintptr_t resolve_symbol(const LoadedElf& elf, uintptr_t plt_entry)
{
	if (elf.pltrel == DT_REL)
		return handle_relocation(elf, *reinterpret_cast<ElfNativeRelocation*>(elf.jmprel + plt_entry));
	if (elf.pltrel == DT_RELA)
		return handle_relocation(elf, reinterpret_cast<ElfNativeRelocationA*>(elf.jmprel)[plt_entry]);
	print_error_and_exit("invalid value for DT_PLTREL", 0);
}

static LoadedElf& load_elf(const char* path, int fd);

static void handle_dynamic(LoadedElf& elf)
{
	uintptr_t pltgot = 0;

	for (size_t i = 0;; i++)
	{
		auto& dynamic = elf.dynamics[i];
		if (dynamic.d_tag == DT_NULL)
			break;

		switch (dynamic.d_tag)
		{
			case DT_PLTGOT:     dynamic.d_un.d_ptr += elf.base; break;
			case DT_HASH:       dynamic.d_un.d_ptr += elf.base; break;
			case DT_STRTAB:     dynamic.d_un.d_ptr += elf.base; break;
			case DT_SYMTAB:     dynamic.d_un.d_ptr += elf.base; break;
			case DT_RELA:       dynamic.d_un.d_ptr += elf.base; break;
			case DT_INIT:       dynamic.d_un.d_ptr += elf.base; break;
			case DT_FINI:       dynamic.d_un.d_ptr += elf.base; break;
			case DT_REL:        dynamic.d_un.d_ptr += elf.base; break;
			case DT_JMPREL:     dynamic.d_un.d_ptr += elf.base; break;
			case DT_INIT_ARRAY: dynamic.d_un.d_ptr += elf.base; break;
			case DT_FINI_ARRAY: dynamic.d_un.d_ptr += elf.base; break;
		}

		switch (dynamic.d_tag)
		{
			case DT_PLTRELSZ:     elf.pltrelsz     = dynamic.d_un.d_val; break;
			case DT_PLTGOT:       pltgot           = dynamic.d_un.d_ptr; break;
			case DT_HASH:         elf.hash         = dynamic.d_un.d_ptr; break;
			case DT_STRTAB:       elf.strtab       = dynamic.d_un.d_ptr; break;
			case DT_SYMTAB:       elf.symtab       = dynamic.d_un.d_ptr; break;
			case DT_RELA:         elf.rela         = dynamic.d_un.d_ptr; break;
			case DT_RELASZ:       elf.relasz       = dynamic.d_un.d_val; break;
			case DT_RELAENT:      elf.relaent      = dynamic.d_un.d_val; break;
			case DT_SYMENT:       elf.syment       = dynamic.d_un.d_val; break;
			case DT_REL:          elf.rel          = dynamic.d_un.d_ptr; break;
			case DT_RELSZ:        elf.relsz        = dynamic.d_un.d_val; break;
			case DT_RELENT:       elf.relent       = dynamic.d_un.d_val; break;
			case DT_PLTREL:       elf.pltrel       = dynamic.d_un.d_val; break;
			case DT_JMPREL:       elf.jmprel       = dynamic.d_un.d_ptr; break;
			case DT_INIT:         elf.init         = dynamic.d_un.d_ptr; break;
			case DT_INIT_ARRAY:   elf.init_array   = dynamic.d_un.d_ptr; break;
			case DT_INIT_ARRAYSZ: elf.init_arraysz = dynamic.d_un.d_val; break;
		}
	}

	for (size_t i = 0;; i++)
	{
		auto& dynamic = elf.dynamics[i];
		if (dynamic.d_tag == DT_NULL)
			break;
		if (dynamic.d_tag != DT_NEEDED)
			continue;

		const char* library_dir = "/usr/lib/";

		char path_buffer[PATH_MAX];
		char* path_ptr = path_buffer;

		const char* library_name = reinterpret_cast<const char*>(elf.strtab + dynamic.d_un.d_val);
		if (library_name[0] != '/')
			for (size_t i = 0; library_dir[i]; i++)
				*path_ptr++ = library_dir[i];
		for (size_t i = 0; library_name[i]; i++)
			*path_ptr++ = library_name[i];
		*path_ptr = '\0';

		char realpath[PATH_MAX];
		if (auto ret = syscall(SYS_REALPATH, path_buffer, realpath); ret < 0)
			print_error_and_exit("realpath", ret);

		int library_fd = syscall(SYS_OPEN, realpath, O_RDONLY);
		if (library_fd < 0)
			print_error_and_exit("could not open library", library_fd);

		const auto& loaded_elf = load_elf(realpath, library_fd);
		dynamic.d_un.d_ptr = reinterpret_cast<uintptr_t>(&loaded_elf);

		syscall(SYS_CLOSE, library_fd);
	}

	// setup required GOT entries
	reinterpret_cast<uintptr_t*>(pltgot)[0] = reinterpret_cast<uintptr_t>(elf.dynamics);
	reinterpret_cast<uintptr_t*>(pltgot)[1] = reinterpret_cast<uintptr_t>(&elf);
	reinterpret_cast<uintptr_t*>(pltgot)[2] = reinterpret_cast<uintptr_t>(&resolve_symbol_trampoline);
}

static bool can_load_elf(int fd, const ElfNativeFileHeader& file_header, uintptr_t base)
{
	for (size_t i = 0; i < file_header.e_phnum; i++)
	{
		ElfNativeProgramHeader program_header;
		if (auto ret = syscall(SYS_PREAD, fd, &program_header, sizeof(program_header), file_header.e_phoff + i * file_header.e_phentsize); ret != sizeof(program_header))
			print_error_and_exit("could not read program header", ret);
		program_header.p_vaddr += base;

		uintptr_t page_alinged_vaddr = program_header.p_vaddr & ~(uintptr_t)0xFFF;
		size_t mmap_length = (program_header.p_vaddr + program_header.p_memsz) - page_alinged_vaddr;

		sys_mmap_t mmap_args;
		mmap_args.addr = reinterpret_cast<void*>(page_alinged_vaddr);
		mmap_args.fildes = -1;
		mmap_args.flags = MAP_FIXED | MAP_ANONYMOUS | MAP_PRIVATE;
		mmap_args.len = mmap_length;
		mmap_args.off = 0;
		mmap_args.prot = PROT_NONE;

		auto ret = reinterpret_cast<void*>(syscall(SYS_MMAP, &mmap_args));
		if (ret == MAP_FAILED)
			return false;
		syscall(SYS_MUNMAP, ret, mmap_length);
	}

	return true;
}

static void load_program_header(const ElfNativeProgramHeader& program_header, int fd, bool needs_writable)
{
	if (program_header.p_type != PT_LOAD)
		print_error_and_exit("trying to load non PT_LOAD program header", 0);
	if (program_header.p_memsz < program_header.p_filesz)
		print_error_and_exit("invalid program header, memsz lower than filesz", 0);

	const int prot =
		[&program_header]() -> int
		{
			int result = 0;
			if (program_header.p_flags & PF_R)
				result |= PROT_READ;
			if (program_header.p_flags & PF_W)
				result |= PROT_WRITE;
			if (program_header.p_flags & PF_X)
				result |= PROT_EXEC;
			return result;
		}();

	if ((program_header.p_vaddr & 0xFFF) || (program_header.p_offset & 0xFFF) || program_header.p_filesz == 0)
	{
		const uintptr_t aligned_addr = program_header.p_vaddr & ~(uintptr_t)0xFFF;

		// unaligned addresses, cannot use file mmap
		sys_mmap_t mmap_args;
		mmap_args.addr = reinterpret_cast<void*>(aligned_addr);
		mmap_args.fildes = -1;
		mmap_args.flags = MAP_ANONYMOUS | MAP_PRIVATE | MAP_FIXED;
		mmap_args.len = (program_header.p_vaddr + program_header.p_memsz) - aligned_addr;
		mmap_args.off = 0;
		mmap_args.prot = prot | PROT_WRITE;

		if (auto ret = syscall(SYS_MMAP, &mmap_args); ret != static_cast<long>(aligned_addr))
			print_error_and_exit("could not load program header", ret);

		const uintptr_t addr = program_header.p_vaddr;
		const uintptr_t size = program_header.p_filesz;
		const size_t offset = program_header.p_offset;
		if (auto ret = syscall(SYS_PREAD, fd, addr, size, offset); ret != static_cast<long>(size))
			print_error_and_exit("could not load program header", ret);
	}
	else
	{
		// aligned addresses, use file mmap
		sys_mmap_t mmap_args;
		mmap_args.addr = reinterpret_cast<void*>(program_header.p_vaddr);
		mmap_args.fildes = fd;
		mmap_args.flags = MAP_PRIVATE | MAP_FIXED;
		mmap_args.len = program_header.p_memsz;
		mmap_args.off = program_header.p_offset;
		mmap_args.prot = prot | PROT_WRITE;

		if (auto ret = syscall(SYS_MMAP, &mmap_args); ret != static_cast<long>(program_header.p_vaddr))
			print_error_and_exit("could not load program header", ret);
	}

	if (program_header.p_filesz != program_header.p_memsz)
	{
		memset(
			reinterpret_cast<void*>(program_header.p_vaddr + program_header.p_filesz),
			0x00,
			program_header.p_memsz - program_header.p_filesz
		);
	}

	if (!(prot & PROT_WRITE) && !needs_writable)
	{
		// FIXME: Implement mprotect so PROT_WRITE can be removed
		//syscall(SYS_MPROTECT, start_vaddr, length, prot);
	}
}

static LoadedElf s_loaded_files[128];
static size_t s_loaded_file_count = 0;

static LoadedElf* get_libc_elf()
{
	for (size_t i = 0; i < s_loaded_file_count; i++)
		if (strcmp(s_loaded_files[i].path, "/usr/lib/libc.so") == 0)
			return &s_loaded_files[i];
	return nullptr;
}

static LoadedElf* get_libgcc_elf()
{
	for (size_t i = 0; i < s_loaded_file_count; i++)
		if (strcmp(s_loaded_files[i].path, "/usr/lib/libgcc_s.so") == 0)
			return &s_loaded_files[i];
	return nullptr;
}

static LoadedElf& load_elf(const char* path, int fd)
{
	for (size_t i = 0; i < s_loaded_file_count; i++)
		if (strcmp(s_loaded_files[i].path, path) == 0)
			return s_loaded_files[i];

	ElfNativeFileHeader file_header;
	if (auto ret = syscall(SYS_READ, fd, &file_header, sizeof(file_header)); ret != sizeof(file_header))
		print_error_and_exit("could not read file header", ret);

	validate_program_header(file_header);

	uintptr_t base = 0;
	if (file_header.e_type == ET_DYN)
	{
#if defined(__x86_64__)
		constexpr uintptr_t base_mask = 0x7FFFFFFFF000;
#elif defined(__i686__)
		constexpr uintptr_t base_mask = 0x7FFFF000;
#else
		#error "unsupported architecture"
#endif

		// FIXME: This is very hacky :D
		do {
			base = (get_random_uptr() & base_mask) + 0x100000;
		} while (!can_load_elf(fd, file_header, base));
	}

	bool needs_writable = false;
	bool has_dynamic_pheader = false;
	ElfNativeProgramHeader dynamic_pheader;
	for (size_t i = 0; i < file_header.e_phnum; i++)
	{
		if (auto ret = syscall(SYS_PREAD, fd, &dynamic_pheader, sizeof(dynamic_pheader), file_header.e_phoff + i * file_header.e_phentsize); ret != sizeof(dynamic_pheader))
			print_error_and_exit("could not read program header", ret);
		if (dynamic_pheader.p_type != PT_DYNAMIC)
			continue;

		sys_mmap_t mmap_args;
		mmap_args.addr = nullptr;
		mmap_args.fildes = -1;
		mmap_args.flags = MAP_ANONYMOUS | MAP_PRIVATE;
		mmap_args.len = dynamic_pheader.p_memsz;
		mmap_args.off = 0;
		mmap_args.prot = PROT_READ | PROT_WRITE;

		const auto uaddr = syscall(SYS_MMAP, &mmap_args);
		if (uaddr < 0)
			print_error_and_exit("could not map dynamic header", uaddr);
		if (auto ret = syscall(SYS_PREAD, fd, uaddr, dynamic_pheader.p_filesz, dynamic_pheader.p_offset); ret != static_cast<long>(dynamic_pheader.p_filesz))
			print_error_and_exit("could not read dynamic header", ret);

		const auto* dynamics = reinterpret_cast<ElfNativeDynamic*>(uaddr);
		for (size_t j = 0;; j++)
		{
			const auto& dynamic = dynamics[j];
			if (dynamic.d_tag == DT_NULL)
				break;
			if (dynamic.d_tag != DT_TEXTREL)
				continue;
			needs_writable = true;
			break;
		}

		syscall(SYS_MUNMAP, uaddr, dynamic_pheader.p_memsz);

		has_dynamic_pheader = true;
		break;
	}

	for (size_t i = 0; i < file_header.e_phnum; i++)
	{
		ElfNativeProgramHeader program_header;
		if (auto ret = syscall(SYS_PREAD, fd, &program_header, sizeof(program_header), file_header.e_phoff + i * file_header.e_phentsize); ret != sizeof(program_header))
			print_error_and_exit("could not read program header", ret);

		switch (program_header.p_type)
		{
			case PT_NULL:
			case PT_DYNAMIC:
			case PT_INTERP:
			case PT_NOTE:
			case PT_PHDR:
				break;
			case PT_LOAD:
				program_header.p_vaddr += base;
				load_program_header(program_header, fd, needs_writable);
				break;
			default:
				print(STDERR_FILENO, "unsupported program header type ");
				print_uint(STDERR_FILENO, program_header.p_type);
				print_error_and_exit("", 0);
		}
	}

	auto& elf = s_loaded_files[s_loaded_file_count++];
	elf.base = base;
	elf.dynamics = nullptr;
	memcpy(&elf.file_header, &file_header, sizeof(file_header));
	strcpy(elf.path, path);

	if (has_dynamic_pheader)
	{
		sys_mmap_t mmap_args;
		mmap_args.addr = nullptr;
		mmap_args.fildes = -1;
		mmap_args.flags = MAP_ANONYMOUS | MAP_PRIVATE;
		mmap_args.len = dynamic_pheader.p_memsz;
		mmap_args.off = 0;
		mmap_args.prot = PROT_READ | PROT_WRITE;

		const auto uaddr = syscall(SYS_MMAP, &mmap_args);
		if (uaddr < 0)
			print_error_and_exit("could not map dynamic header", uaddr);
		if (auto ret = syscall(SYS_PREAD, fd, uaddr, dynamic_pheader.p_filesz, dynamic_pheader.p_offset); ret != static_cast<long>(dynamic_pheader.p_filesz))
			print_error_and_exit("could not read dynamic header", ret);

		elf.dynamics = reinterpret_cast<ElfNativeDynamic*>(uaddr);
		handle_dynamic(elf);
	}

	return elf;
}

static void call_init_funcs(LoadedElf& elf, char** envp, bool skip)
{
	if (elf.has_called_init)
		return;

	if (elf.dynamics)
	{
		for (size_t i = 0;; i++)
		{
			const auto& dynamic = elf.dynamics[i];
			if (dynamic.d_tag == DT_NULL)
				break;
			if (dynamic.d_tag == DT_NEEDED)
				call_init_funcs(*reinterpret_cast<LoadedElf*>(dynamic.d_un.d_ptr), envp, false);
		}
	}

	if (elf.has_called_init || skip)
		return;

	using init_t = void(*)();
	if (elf.init)
		reinterpret_cast<init_t>(elf.init)();
	for (size_t i = 0; i < elf.init_arraysz / sizeof(init_t); i++)
		reinterpret_cast<init_t*>(elf.init_array)[i]();

	if (strcmp(elf.path, "/usr/lib/libc.so") == 0)
	{
		const uintptr_t init_libc = get_symbol_address(elf, "_init_libc");
		if (init_libc != SYM_NOT_FOUND)
		{
			using init_libc_t = void(*)(char**);
			reinterpret_cast<init_libc_t>(init_libc)(envp);
		}
	}

	elf.has_called_init = true;
}

extern "C"
__attribute__((used, noreturn))
int _entry(int argc, char** argv, char** envp, int fd)
{
	const bool invoked_directly = (fd < 0);
	if (invoked_directly)
	{
		if (argc < 2)
			print_error_and_exit("missing program name", 0);

		argc--;
		argv++;

		fd = syscall(SYS_OPEN, argv[0], O_RDONLY);
		if (fd < 0)
			print_error_and_exit("could not open program", fd);
	}

	init_random();
	auto elf = load_elf(argv[0], fd);
	syscall(SYS_CLOSE, fd);
	fini_random();

	relocate_elf(elf, true);
	call_init_funcs(elf, envp, true);
	call_entry_point(argc, argv, envp, elf.base + elf.file_header.e_entry);
}
