#include "utils.h"

#include <LibELF/AuxiliaryVector.h>
#include <LibELF/Types.h>
#include <LibELF/Values.h>

#include <BAN/Atomic.h>

#include <dlfcn.h>
#include <fcntl.h>
#include <limits.h>
#include <pthread.h>
#include <sys/mman.h>
#include <sys/syscall.h>
#include <unistd.h>

#if defined(__x86_64__)
	#define ELF_R_SYM ELF64_R_SYM
#elif defined(__i686__)
	#define ELF_R_SYM ELF32_R_SYM
#endif

extern "C"
__attribute__((naked))
void _start()
{
#if defined(__x86_64__)
	asm volatile(
		"movq  (%rsp), %rdi;"
		"leaq 8(%rsp), %rsi;"
		"leaq 8(%rsi, %rdi, 8), %rdx;"

		"movq %rsp, %rbp;"
		"andq $-16, %rsp;"

		"call _entry;"

		"movq %rbp, %rsp;"
		"xorq %rbp, %rbp;"

		"jmp *%rax;"

		"ud2;"
	);
#elif defined(__i686__)
	asm volatile(
		"movl  (%esp), %edi;"
		"leal 4(%esp), %esi;"
		"leal 4(%esi, %edi, 4), %edx;"

		"movl %esp, %ebp;"
		"andl $-16, %esp;"

		"subl $4, %esp;"
		"pushl %edx;"
		"pushl %esi;"
		"pushl %edi;"

		"call _entry;"

		"movl %ebp, %esp;"
		"xorl %ebp, %ebp;"

		"jmp *%eax;"

		"ud2;"
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
	ElfNativeProgramHeader tls_header;
	ElfNativeDynamic* dynamics;

	uint8_t* tls_addr;
	size_t tls_module;
	size_t tls_offset;

	int fd;

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

	uintptr_t fini;
	uintptr_t fini_array;
	size_t fini_arraysz;

	bool is_calling_init;
	bool is_registering_fini;
	bool is_relocating;

	char path[PATH_MAX];

	struct LoadedPHDR
	{
		uintptr_t base;
		size_t size;
	};
	LoadedPHDR loaded_phdrs[16];
	size_t loaded_phdr_count;

	size_t real_symtab_size;
	size_t real_symtab_entsize;
	const uint8_t* real_symtab_addr;
	const uint8_t* real_strtab_addr;
};

static LoadedElf s_loaded_files[128];
static size_t s_loaded_file_count = 0;

static const char* s_ld_library_path = nullptr;

static BAN::Atomic<pthread_t> s_global_locker = 0;
static uint32_t s_global_lock_depth = 0;

constexpr uintptr_t SYM_NOT_FOUND = -1;

static void lock_global_lock()
{
	const pthread_t tid = syscall<>(SYS_PTHREAD_SELF);

	pthread_t expected = 0;
	while (!s_global_locker.compare_exchange(expected, tid))
	{
		if (expected == tid)
			break;
		syscall<>(SYS_YIELD);
		expected = 0;
	}

	s_global_lock_depth++;
}

static void unlock_global_lock()
{
	s_global_lock_depth--;
	if (s_global_lock_depth == 0)
		s_global_locker.store(false);
}

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

template<typename RelocT> requires BAN::is_same_v<RelocT, ElfNativeRelocation> || BAN::is_same_v<RelocT, ElfNativeRelocationA>
static bool is_tls_relocation(const RelocT& reloc)
{
#if defined(__x86_64__)
	switch (ELF64_R_TYPE(reloc.r_info))
	{
		case R_X86_64_DTPMOD64:
		case R_X86_64_DTPOFF64:
		case R_X86_64_TPOFF64:
		case R_X86_64_TLSGD:
		case R_X86_64_TLSLD:
		case R_X86_64_DTPOFF32:
		case R_X86_64_GOTTPOFF:
		case R_X86_64_TPOFF32:
			return true;
	}
#elif defined(__i686__)
	switch (ELF32_R_TYPE(reloc.r_info))
	{
		case R_386_TLS_TPOFF:
		case R_386_TLS_IE:
		case R_386_TLS_GOTIE:
		case R_386_TLS_LE:
		case R_386_TLS_GD:
		case R_386_TLS_LDM:
		case R_386_TLS_GD_32:
		case R_386_TLS_GD_PUSH:
		case R_386_TLS_GD_CALL:
		case R_386_TLS_GD_POP:
		case R_386_TLS_LDM_32:
		case R_386_TLS_LDM_PUSH:
		case R_386_TLS_LDM_CALL:
		case R_386_TLS_LDM_POP:
		case R_386_TLS_LDO_32:
		case R_386_TLS_IE_32:
		case R_386_TLS_LE_32:
		case R_386_TLS_DTPMOD32:
		case R_386_TLS_DTPOFF32:
		case R_386_TLS_TPOFF32:
			return true;
	}
#else
	#error "unsupported architecture"
#endif
	return false;
}

template<typename RelocT> requires BAN::is_same_v<RelocT, ElfNativeRelocation> || BAN::is_same_v<RelocT, ElfNativeRelocationA>
static bool is_copy_relocation(const RelocT& reloc)
{
#if defined(__x86_64__)
	return ELF64_R_TYPE(reloc.r_info) == R_X86_64_COPY;
#elif defined(__i686__)
	return ELF32_R_TYPE(reloc.r_info) == R_386_COPY;
#else
	#error "unsupported architecture"
#endif
}

template<typename RelocT> requires BAN::is_same_v<RelocT, ElfNativeRelocation> || BAN::is_same_v<RelocT, ElfNativeRelocationA>
static void handle_copy_relocation(const LoadedElf& elf, const RelocT& reloc)
{
	if (!is_copy_relocation(reloc))
		return;

	const uint32_t symbol_index = ELF_R_SYM(reloc.r_info);
	if (symbol_index == 0)
		print_error_and_exit("copy relocation without a symbol", 0);

	const auto& symbol = *reinterpret_cast<ElfNativeSymbol*>(elf.symtab + symbol_index * elf.syment);
	const char* symbol_name = reinterpret_cast<const char*>(elf.strtab + symbol.st_name);

	ElfNativeSymbol* src_sym = nullptr;
	const LoadedElf* src_elf = nullptr;

	for (size_t i = 0; i < s_loaded_file_count; i++)
	{
		if (&elf == &s_loaded_files[i])
			continue;
		auto* match = find_symbol(s_loaded_files[i], symbol_name);
		if (match == nullptr)
			continue;
		if (ELF_ST_BIND(match->st_info) == STB_LOCAL && &s_loaded_files[i] != &elf)
			continue;
		if (src_sym == nullptr || ELF_ST_BIND(match->st_info) != STB_WEAK)
		{
			src_sym = match;
			src_elf = &s_loaded_files[i];
		}
		if (ELF_ST_BIND(match->st_info) != STB_WEAK)
			break;
	}

	if (src_sym == nullptr)
		print_error_and_exit("copy relocation source not found", 0);

	memcpy(
		reinterpret_cast<void*>(elf.base + reloc.r_offset),
		reinterpret_cast<void*>(src_elf->base + src_sym->st_value),
		symbol.st_size
	);

	src_sym->st_value = (elf.base + reloc.r_offset) - src_elf->base;
}

template<typename RelocT> requires BAN::is_same_v<RelocT, ElfNativeRelocation> || BAN::is_same_v<RelocT, ElfNativeRelocationA>
static void handle_tls_relocation(const LoadedElf& elf, const RelocT& reloc)
{
	if (!is_tls_relocation(reloc))
		return;

	const LoadedElf* symbol_elf = &elf;
	uintptr_t symbol_offset = 0;

	if (const uint32_t symbol_index = ELF_R_SYM(reloc.r_info))
	{
		const auto& symbol = *reinterpret_cast<ElfNativeSymbol*>(elf.symtab + symbol_index * elf.syment);
		const char* symbol_name = reinterpret_cast<const char*>(elf.strtab + symbol.st_name);

		if (symbol.st_shndx && ELF_ST_BIND(symbol.st_info) != STB_WEAK)
			symbol_offset = symbol.st_value;
		else
		{
			symbol_elf = nullptr;
			for (size_t i = 0; i < s_loaded_file_count; i++)
			{
				const auto* match = find_symbol(s_loaded_files[i], symbol_name);
				if (match == nullptr)
					continue;
				if (ELF_ST_BIND(match->st_info) == STB_LOCAL && &s_loaded_files[i] != &elf)
					continue;
				if (symbol_elf == nullptr || ELF_ST_BIND(match->st_info) != STB_WEAK)
				{
					symbol_elf = &s_loaded_files[i];
					symbol_offset = match->st_value;
				}
				if (ELF_ST_BIND(match->st_info) != STB_WEAK)
					break;
			}

			if (symbol_elf == nullptr && ELF_ST_BIND(symbol.st_info) != STB_WEAK)
			{
				print(STDERR_FILENO, elf.path);
				print(STDERR_FILENO, ": could not find symbol \"");
				print(STDERR_FILENO, symbol_name);
				print_error_and_exit("\"", 0);
			}

			if (ELF_ST_TYPE(symbol.st_info) != STT_TLS)
				print_error_and_exit("i don't think this is supposed to happen 1", 0);
		}
	}

	if (symbol_elf == nullptr)
		print_error_and_exit("i don't think this is supposed to happen 2", 0);

	if (symbol_elf->tls_addr == nullptr)
		print_error_and_exit("i don't think this is supposed to happen 3", 0);

#if defined(__x86_64__)
	switch (ELF64_R_TYPE(reloc.r_info))
	{
		case R_X86_64_DTPMOD64:
			*reinterpret_cast<uint64_t*>(elf.base + reloc.r_offset) = symbol_elf->tls_module;
			break;
		case R_X86_64_DTPOFF64:
			*reinterpret_cast<uint64_t*>(elf.base + reloc.r_offset) = symbol_offset;
			break;
		case R_X86_64_TPOFF64:
			*reinterpret_cast<uint64_t*>(elf.base + reloc.r_offset) = symbol_offset - symbol_elf->tls_offset;
			break;
		default:
			print(STDERR_FILENO, "unsupported tls reloc type ");
			print_uint(STDERR_FILENO, ELF64_R_TYPE(reloc.r_info));
			print(STDERR_FILENO, " in ");
			print(STDERR_FILENO, elf.path);
			print_error_and_exit("", 0);
	}
#elif defined(__i686__)
	switch (ELF32_R_TYPE(reloc.r_info))
	{
		case R_386_TLS_TPOFF:
			*reinterpret_cast<uint32_t*>(elf.base + reloc.r_offset) = symbol_offset - symbol_elf->tls_offset;
			break;
		case R_386_TLS_DTPMOD32:
			*reinterpret_cast<uint32_t*>(elf.base + reloc.r_offset) = symbol_elf->tls_module;
			break;
		case R_386_TLS_DTPOFF32:
			*reinterpret_cast<uint32_t*>(elf.base + reloc.r_offset) = symbol_offset;
			break;
		default:
			print(STDERR_FILENO, "unsupported tls reloc type ");
			print_uint(STDERR_FILENO, ELF32_R_TYPE(reloc.r_info));
			print(STDERR_FILENO, " in ");
			print(STDERR_FILENO, elf.path);
			print_error_and_exit("", 0);
	}
#else
	#error "unsupported architecture"
#endif
}

extern "C" int __dlclose(void* handle);
extern "C" char* __dlerror(void);
extern "C" void* __dlopen(const char* file, int mode);
extern "C" void* __dlsym(void* __restrict handle, const char* __restrict name);
extern "C" int __dladdr(const void* addr, Dl_info_t* dlip);

template<typename RelocT> requires BAN::is_same_v<RelocT, ElfNativeRelocation> || BAN::is_same_v<RelocT, ElfNativeRelocationA>
static uintptr_t handle_relocation(const LoadedElf& elf, const RelocT& reloc, bool resolve_symbols)
{
	if (is_copy_relocation(reloc) || is_tls_relocation(reloc))
		return 0;

	const uint32_t symbol_index = ELF_R_SYM(reloc.r_info);
	if (resolve_symbols == !symbol_index)
		return 0;

	uintptr_t symbol_address = 0;
	if (symbol_index)
	{
		const auto& symbol = *reinterpret_cast<ElfNativeSymbol*>(elf.symtab + symbol_index * elf.syment);
		const char* symbol_name = reinterpret_cast<const char*>(elf.strtab + symbol.st_name);

		if (false) {}
#define CHECK_SYM(sym) \
		else if (strcmp(symbol_name, #sym) == 0) \
			symbol_address = reinterpret_cast<uintptr_t>(&sym)
		CHECK_SYM(__dlclose);
		CHECK_SYM(__dlerror);
		CHECK_SYM(__dlopen);
		CHECK_SYM(__dlsym);
		CHECK_SYM(__dladdr);
#undef CHECK_SYM
		else if (symbol.st_shndx && ELF_ST_BIND(symbol.st_info) != STB_WEAK)
			symbol_address = elf.base + symbol.st_value;
		else
		{
			symbol_address = SYM_NOT_FOUND;
			for (size_t i = 0; i < s_loaded_file_count; i++)
			{
				const auto* match = find_symbol(s_loaded_files[i], symbol_name);
				if (match == nullptr)
					continue;
				if (ELF_ST_BIND(match->st_info) == STB_LOCAL && &s_loaded_files[i] != &elf)
					continue;
				if (symbol_address == SYM_NOT_FOUND || ELF_ST_BIND(match->st_info) != STB_WEAK)
					symbol_address = s_loaded_files[i].base + match->st_value;
				if (ELF_ST_BIND(match->st_info) != STB_WEAK)
					break;
			}

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

		if (ELF_ST_TYPE(symbol.st_info) == STT_TLS)
			print_error_and_exit("relocating TLS symbol", 0);
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
			print(STDERR_FILENO, " in ");
			print(STDERR_FILENO, elf.path);
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
	if (elf.is_relocating)
		return;
	elf.is_relocating = true;

	// do copy relocations
	if (elf.rel && elf.relent)
		for (size_t i = 0; i < elf.relsz / elf.relent; i++)
			handle_copy_relocation(elf, *reinterpret_cast<ElfNativeRelocation*>(elf.rel + i * elf.relent));
	if (elf.rela && elf.relaent)
		for (size_t i = 0; i < elf.relasz / elf.relaent; i++)
			handle_copy_relocation(elf, *reinterpret_cast<ElfNativeRelocationA*>(elf.rela + i * elf.relaent));

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

	// do "normal" relocations
	if (elf.rel && elf.relent)
		for (size_t i = 0; i < elf.relsz / elf.relent; i++)
			handle_relocation(elf, *reinterpret_cast<ElfNativeRelocation*>(elf.rel + i * elf.relent), true);
	if (elf.rela && elf.relaent)
		for (size_t i = 0; i < elf.relasz / elf.relaent; i++)
			handle_relocation(elf, *reinterpret_cast<ElfNativeRelocationA*>(elf.rela + i * elf.relaent), true);

	// do tls relocations
	if (elf.rel && elf.relent)
		for (size_t i = 0; i < elf.relsz / elf.relent; i++)
			handle_tls_relocation(elf, *reinterpret_cast<ElfNativeRelocation*>(elf.rel + i * elf.relent));
	if (elf.rela && elf.relaent)
		for (size_t i = 0; i < elf.relasz / elf.relaent; i++)
			handle_tls_relocation(elf, *reinterpret_cast<ElfNativeRelocationA*>(elf.rela + i * elf.relaent));

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
						handle_relocation(elf, reinterpret_cast<ElfNativeRelocation*>(elf.jmprel)[i], true);
					break;
				case DT_RELA:
					for (size_t i = 0; i < elf.pltrelsz / sizeof(ElfNativeRelocationA); i++)
						handle_relocation(elf, reinterpret_cast<ElfNativeRelocationA*>(elf.jmprel)[i], true);
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

				bool do_relocation = false;

				if (const uint32_t symbol_index = ELF_R_SYM(info))
				{
					const auto& symbol = *reinterpret_cast<ElfNativeSymbol*>(elf.symtab + symbol_index * elf.syment);
					const char* symbol_name = reinterpret_cast<const char*>(elf.strtab + symbol.st_name);
					if (strcmp(symbol_name, "__tls_get_addr") == 0 || strcmp(symbol_name, "___tls_get_addr") == 0)
						do_relocation = true;
				}

				if (!do_relocation)
					*reinterpret_cast<uintptr_t*>(elf.base + offset) += elf.base;
				else switch (elf.pltrel)
				{
					case DT_REL:
						handle_relocation(elf, reinterpret_cast<ElfNativeRelocation*>(elf.jmprel)[i], true);
						break;
					case DT_RELA:
						handle_relocation(elf, reinterpret_cast<ElfNativeRelocationA*>(elf.jmprel)[i], true);
						break;
				}
			}
		}
	}
}

extern "C"
__attribute__((used))
uintptr_t resolve_symbol(const LoadedElf& elf, uintptr_t plt_entry)
{
	lock_global_lock();

	uintptr_t result;
	switch (elf.pltrel)
	{
		case DT_REL:
			result = handle_relocation(elf, *reinterpret_cast<ElfNativeRelocation*>(elf.jmprel + plt_entry), true);
			break;
		case DT_RELA:
			result = handle_relocation(elf, reinterpret_cast<ElfNativeRelocationA*>(elf.jmprel)[plt_entry], true);
			break;
		default:
			print_error_and_exit("invalid value for DT_PLTREL", 0);
	}

	unlock_global_lock();

	return result;
}

static LoadedElf& load_elf(const char* path, int fd);

static bool check_library(const char* library_dir, const char* library_name, char out[PATH_MAX])
{
	char path_buffer[PATH_MAX];
	char* path_ptr = path_buffer;

	if (library_name[0] != '/')
		for (size_t i = 0; library_dir[i]; i++)
			*path_ptr++ = library_dir[i];
	*path_ptr++ = '/';
	for (size_t i = 0; library_name[i]; i++)
		*path_ptr++ = library_name[i];
	*path_ptr = '\0';

	return syscall(SYS_REALPATH, path_buffer, out) >= 0;
}

static bool find_library(const char* library_name, char out[PATH_MAX])
{
	if (s_ld_library_path && check_library(s_ld_library_path, library_name, out))
		return true;
	if (check_library("/usr/lib", library_name, out))
		return true;
	return false;
}

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
			case DT_PLTGOT:
			case DT_HASH:
			case DT_STRTAB:
			case DT_SYMTAB:
			case DT_RELA:
			case DT_INIT:
			case DT_FINI:
			case DT_REL:
			case DT_JMPREL:
			case DT_INIT_ARRAY:
			case DT_FINI_ARRAY:
				dynamic.d_un.d_ptr += elf.base;
				break;
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
			case DT_FINI:         elf.fini         = dynamic.d_un.d_ptr; break;
			case DT_FINI_ARRAY:   elf.fini_array   = dynamic.d_un.d_ptr; break;
			case DT_FINI_ARRAYSZ: elf.fini_arraysz = dynamic.d_un.d_val; break;
		}
	}

	for (size_t i = 0;; i++)
	{
		auto& dynamic = elf.dynamics[i];
		if (dynamic.d_tag == DT_NULL)
			break;
		if (dynamic.d_tag != DT_NEEDED)
			continue;

		const char* library_name = reinterpret_cast<const char*>(elf.strtab + dynamic.d_un.d_val);

		char path_buffer[PATH_MAX];
		if (!find_library(library_name, path_buffer))
		{
			print(STDERR_FILENO, "could not open shared object: ");
			print_error_and_exit(library_name, 0);
		}

		const auto& loaded_elf = load_elf(path_buffer, -1);
		dynamic.d_un.d_ptr = reinterpret_cast<uintptr_t>(&loaded_elf);
	}

	// do relocations without symbols
	if (elf.rel && elf.relent)
		for (size_t i = 0; i < elf.relsz / elf.relent; i++)
			handle_relocation(elf, *reinterpret_cast<ElfNativeRelocation*>(elf.rel + i * elf.relent), false);
	if (elf.rela && elf.relaent)
		for (size_t i = 0; i < elf.relasz / elf.relaent; i++)
			handle_relocation(elf, *reinterpret_cast<ElfNativeRelocationA*>(elf.rela + i * elf.relaent), false);

	if (pltgot == 0)
		return;

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

	const size_t file_backed_size =
		[&program_header]() -> size_t
		{
			if ((program_header.p_vaddr & 0xFFF) || (program_header.p_offset & 0xFFF))
				return 0;
			if (program_header.p_filesz == program_header.p_memsz)
				return program_header.p_filesz;
			return program_header.p_filesz & ~(uintptr_t)0xFFF;
		}();

	if (file_backed_size)
	{
		// aligned addresses, use file mmap
		sys_mmap_t mmap_args;
		mmap_args.addr = reinterpret_cast<void*>(program_header.p_vaddr);
		mmap_args.fildes = fd;
		mmap_args.flags = MAP_PRIVATE | MAP_FIXED;
		mmap_args.len = file_backed_size;
		mmap_args.off = program_header.p_offset;
		mmap_args.prot = prot | PROT_WRITE;

		if (auto ret = syscall(SYS_MMAP, &mmap_args); ret != static_cast<long>(program_header.p_vaddr))
			print_error_and_exit("could not load program header", ret);
	}

	if (file_backed_size < program_header.p_memsz)
	{
		const uintptr_t aligned_addr = (program_header.p_vaddr + file_backed_size) & ~(uintptr_t)0xFFF;

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

		if (file_backed_size < program_header.p_filesz)
		{
			const uintptr_t addr = program_header.p_vaddr + file_backed_size;
			const uintptr_t size = program_header.p_filesz - file_backed_size;
			const size_t offset = program_header.p_offset + file_backed_size;
			if (auto ret = syscall(SYS_PREAD, fd, addr, size, offset); ret != static_cast<long>(size))
				print_error_and_exit("could not load program header", ret);
		}

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

static bool read_section_header(const LoadedElf& elf, size_t index, ElfNativeSectionHeader& header)
{
	if (index >= elf.file_header.e_shnum)
		return false;

	const auto& file_header = elf.file_header;
	if (syscall(SYS_PREAD, elf.fd, &header, sizeof(header), file_header.e_shoff + index * file_header.e_shentsize) != sizeof(header))
		return false;

	return true;
}

static const uint8_t* mmap_section_header_data(const LoadedElf& elf, const ElfNativeSectionHeader& header)
{
	const size_t offset_in_page = header.sh_offset % PAGE_SIZE;
	const sys_mmap_t mmap_args {
		.addr = nullptr,
		.len = header.sh_size + offset_in_page,
		.prot = PROT_READ,
		.flags = MAP_SHARED,
		.fildes = elf.fd,
		.off = static_cast<off_t>(header.sh_offset - offset_in_page),
	};

	const uint8_t* mmap_ret = reinterpret_cast<const uint8_t*>(syscall(SYS_MMAP, &mmap_args));
	if (mmap_ret == MAP_FAILED)
		return nullptr;

	return mmap_ret + offset_in_page;
}

static bool load_symbol_table(LoadedElf& elf)
{
	if (elf.file_header.e_shentsize < sizeof(ElfNativeSectionHeader))
		return false;

	for (size_t i = 0; i < elf.file_header.e_shnum; i++)
	{
		ElfNativeSectionHeader symtab_header;
		if (!read_section_header(elf, i, symtab_header))
			return false;
		if (symtab_header.sh_type != SHT_SYMTAB)
			continue;

		ElfNativeSectionHeader strtab_header;
		if (!read_section_header(elf, symtab_header.sh_link, strtab_header))
			return false;

		elf.real_symtab_entsize = symtab_header.sh_entsize;
		elf.real_symtab_size = symtab_header.sh_size;
		elf.real_symtab_addr = mmap_section_header_data(elf, symtab_header);
		elf.real_strtab_addr = mmap_section_header_data(elf, strtab_header);
		return true;
	}

	return false;
}

static LoadedElf& load_elf(const char* path, int fd)
{
	lock_global_lock();

	for (size_t i = 0; i < s_loaded_file_count; i++)
	{
		if (strcmp(s_loaded_files[i].path, path) == 0)
		{
			unlock_global_lock();
			return s_loaded_files[i];
		}
	}

	if (fd == -1 && (fd = syscall(SYS_OPENAT, AT_FDCWD, path, O_RDONLY)) < 0)
		print_error_and_exit("could not open library", fd);

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
		do
			base = (get_random_uptr() & base_mask) + 0x100000;
		while (!can_load_elf(fd, file_header, base));
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

	auto& elf = s_loaded_files[s_loaded_file_count++];
	elf.tls_header.p_type = PT_NULL;
	elf.base = base;
	elf.fd = fd;
	elf.dynamics = nullptr;
	memcpy(&elf.file_header, &file_header, sizeof(file_header));
	strcpy(elf.path, path);

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
			case PT_GNU_EH_FRAME:
			case PT_GNU_STACK:
			case PT_GNU_RELRO:
				break;
			case PT_TLS:
				elf.tls_header = program_header;
				break;
			case PT_LOAD:
				if (elf.loaded_phdr_count >= sizeof(elf.loaded_phdrs) / sizeof(*elf.loaded_phdrs))
				{
					print(STDERR_FILENO, "file '");
					print(STDERR_FILENO, elf.path);
					print_error_and_exit("' has too many PT_LOAD headers", 0);
				}
				program_header.p_vaddr += base;
				elf.loaded_phdrs[elf.loaded_phdr_count++] = {
					.base = program_header.p_vaddr,
					.size = program_header.p_memsz,
				};
				load_program_header(program_header, fd, needs_writable);
				break;
			default:
				print(STDERR_FILENO, "unsupported program header type ");
				print_uint(STDERR_FILENO, program_header.p_type);
				print_error_and_exit("", 0);
		}
	}


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

	load_symbol_table(elf);

	unlock_global_lock();

	return elf;
}

struct MasterTLS
{
	uint8_t* addr;
	size_t size;
	size_t module_count;
};

static MasterTLS initialize_master_tls()
{
	constexpr auto round =
		[](size_t a, size_t b) -> size_t
		{
			return b * ((a + b - 1) / b);
		};

	size_t max_align = alignof(uthread);
	size_t tls_m_offset = 0;
	size_t tls_m_size = 0;
	size_t module_count = 0;
	for (size_t i = 0; i < s_loaded_file_count; i++)
	{
		const auto& tls_header = s_loaded_files[i].tls_header;
		if (tls_header.p_type != PT_TLS)
			continue;
		if (tls_header.p_align == 0)
			print_error_and_exit("TLS alignment is 0", 0);

		max_align = max<size_t>(max_align, tls_header.p_align);
		tls_m_offset = round(tls_m_offset + tls_header.p_memsz, tls_header.p_align);
		tls_m_size = tls_header.p_memsz;

		module_count++;
	}

	if (module_count == 0)
		return { .addr = nullptr, .size = 0, .module_count = 0 };

	size_t master_tls_size = tls_m_offset + tls_m_size;
	if (auto rem = master_tls_size % max_align)
		master_tls_size += max_align - rem;

	uint8_t* master_tls_addr;

	{
		const sys_mmap_t mmap_args {
			.addr = nullptr,
			.len = master_tls_size,
			.prot = PROT_READ | PROT_WRITE,
			.flags = MAP_ANONYMOUS | MAP_PRIVATE,
			.fildes = -1,
			.off = 0,
		};

		const auto ret = syscall(SYS_MMAP, &mmap_args);
		if (ret < 0)
			print_error_and_exit("failed to allocate master TLS", ret);
		master_tls_addr = reinterpret_cast<uint8_t*>(ret);
	}

	for (size_t i = 0, tls_offset = 0, tls_module = 1; i < s_loaded_file_count; i++)
	{
		const auto& tls_header = s_loaded_files[i].tls_header;
		if (tls_header.p_type != PT_TLS)
			continue;

		tls_offset = round(tls_offset + tls_header.p_memsz, tls_header.p_align);

		uint8_t* tls_buffer = master_tls_addr + master_tls_size - tls_offset;

		if (tls_header.p_filesz > 0)
		{
			const int fd = s_loaded_files[i].fd;
			if (auto ret = syscall(SYS_PREAD, fd, tls_buffer, tls_header.p_filesz, tls_header.p_offset); ret != static_cast<long>(tls_header.p_filesz))
				print_error_and_exit("failed to read TLS data", ret);
		}

		memset(tls_buffer + tls_header.p_filesz, 0, tls_header.p_memsz - tls_header.p_filesz);

		auto& elf = s_loaded_files[i];
		elf.tls_addr = tls_buffer;
		elf.tls_module = tls_module++;
		elf.tls_offset = tls_offset;
	}

	return { .addr = master_tls_addr, .size = master_tls_size, .module_count = module_count };
}

static void initialize_tls(MasterTLS master_tls)
{
	const size_t tls_size = master_tls.size
		+ sizeof(uthread)
		+ (master_tls.module_count + 1) * sizeof(uintptr_t);

	uint8_t* tls_addr;

	{
		const sys_mmap_t mmap_args {
			.addr = nullptr,
			.len = tls_size,
			.prot = PROT_READ | PROT_WRITE,
			.flags = MAP_ANONYMOUS | MAP_PRIVATE,
			.fildes = -1,
			.off = 0,
		};

		const auto ret = syscall(SYS_MMAP, &mmap_args);
		if (ret < 0)
			print_error_and_exit("failed to allocate master TLS", ret);
		tls_addr = reinterpret_cast<uint8_t*>(ret);
	}

	memcpy(tls_addr, master_tls.addr, master_tls.size);

	uthread& uthread = *reinterpret_cast<struct uthread*>(tls_addr + master_tls.size);

	// uthread is prepared in libc init, but some other stuff may be calling pthread functions
	//   for example __cxa_guard_release calls pthread_cond_broadcast
	uthread = {
		.self = &uthread,
		.master_tls_addr = master_tls.addr,
		.master_tls_size = master_tls.size,
		.cleanup_stack = nullptr,
		.id = static_cast<pthread_t>(syscall<>(SYS_PTHREAD_SELF)),
		.errno_ = 0,
		.cancel_type = PTHREAD_CANCEL_DEFERRED,
		.cancel_state = PTHREAD_CANCEL_ENABLE,
		.canceled = false,
	};

	uthread.dtv[0] = master_tls.module_count;
	for (size_t i = 0; i < s_loaded_file_count; i++)
	{
		const auto& elf = s_loaded_files[i];
		if (elf.tls_addr == nullptr)
			continue;
		uthread.dtv[elf.tls_module] = reinterpret_cast<uintptr_t>(tls_addr) + uthread.master_tls_size - elf.tls_offset;
	}

	syscall(SYS_SET_TLS, &uthread);
}

static void initialize_environ(char** envp)
{
	uintptr_t environ = SYM_NOT_FOUND;
	for (size_t i = 0; i < s_loaded_file_count; i++)
	{
		const auto* match = find_symbol(s_loaded_files[i], "environ");
		if (match == nullptr)
			continue;
		if (environ == SYM_NOT_FOUND || ELF_ST_BIND(match->st_info) != STB_WEAK)
			environ = s_loaded_files[i].base + match->st_value;
		if (ELF_ST_BIND(match->st_info) != STB_WEAK)
			break;
	}

	if (environ == SYM_NOT_FOUND)
		return;

	*reinterpret_cast<char***>(environ) = envp;
}

static void call_init_funcs(LoadedElf& elf, bool is_main_elf)
{
	if (elf.is_calling_init)
		return;
	elf.is_calling_init = true;

	if (elf.dynamics)
	{
		for (size_t i = 0;; i++)
		{
			const auto& dynamic = elf.dynamics[i];
			if (dynamic.d_tag == DT_NULL)
				break;
			if (dynamic.d_tag == DT_NEEDED)
				call_init_funcs(*reinterpret_cast<LoadedElf*>(dynamic.d_un.d_ptr), false);
		}
	}

	// main executable calls its init functions in _start
	if (is_main_elf)
		return;

	using init_t = void(*)();
	if (elf.init)
		reinterpret_cast<init_t>(elf.init)();
	for (size_t i = 0; i < elf.init_arraysz / sizeof(init_t); i++)
		reinterpret_cast<init_t*>(elf.init_array)[i]();
}

static uintptr_t find_atexit()
{
	static bool atexit_found = false;
	static uintptr_t atexit = 0;

	if (atexit_found)
		return atexit;

	atexit = SYM_NOT_FOUND;
	for (size_t i = 0; i < s_loaded_file_count; i++)
	{
		const auto* match = find_symbol(s_loaded_files[i], "atexit");
		if (match == nullptr)
			continue;
		if (atexit == SYM_NOT_FOUND || ELF_ST_BIND(match->st_info) != STB_WEAK)
			atexit = s_loaded_files[i].base + match->st_value;
		if (ELF_ST_BIND(match->st_info) != STB_WEAK)
			break;
	}

	if (atexit == SYM_NOT_FOUND)
		atexit = 0;

	atexit_found = true;

	return atexit;

}

static void register_fini_funcs(LoadedElf& elf, bool is_main_elf)
{
	if (elf.is_registering_fini)
		return;
	elf.is_registering_fini = true;

	using fini_t = void(*)();
	using atexit_t = int(*)(fini_t);

	auto atexit = reinterpret_cast<atexit_t>(find_atexit());
	if (atexit == nullptr)
		return;

	// main executable registers its fini functions in _start
	if (!is_main_elf)
	{
		for (size_t i = 0; i < elf.fini_arraysz / sizeof(fini_t); i++)
			atexit(reinterpret_cast<fini_t*>(elf.fini_array)[i]);
		if (elf.fini)
			atexit(reinterpret_cast<fini_t>(elf.fini));
	}

	if (elf.dynamics)
	{
		for (size_t i = 0;; i++)
		{
			const auto& dynamic = elf.dynamics[i];
			if (dynamic.d_tag == DT_NULL)
				break;
			if (dynamic.d_tag == DT_NEEDED)
				register_fini_funcs(*reinterpret_cast<LoadedElf*>(dynamic.d_un.d_ptr), false);
		}
	}
}

int __dlclose(void* handle)
{
	// TODO: maybe actually close handles? (not required by spec)
	(void)handle;
	return 0;
}

static const char* s_dlerror_string = nullptr;

char* __dlerror(void)
{
	const char* result = s_dlerror_string;
	s_dlerror_string = nullptr;
	return const_cast<char*>(result);
}

void* __dlopen(const char* file, int mode)
{
	const bool lazy = !(mode & RTLD_NOW);

	// FIXME: RTLD_{LOCAL,GLOBAL}

	if (file == nullptr)
		return &s_loaded_files[0];

	char path_buffer[PATH_MAX];
	if (!find_library(file, path_buffer))
	{
		s_dlerror_string = "Could not find file";
		return nullptr;
	}

	const size_t old_loaded_count = s_loaded_file_count;

	init_random();
	auto& elf = load_elf(path_buffer, -1);
	fini_random();

	if (!elf.is_relocating && !elf.is_calling_init)
	{
		for (size_t i = old_loaded_count + 1; i < s_loaded_file_count; i++)
		{
			if (s_loaded_files[i].tls_header.p_type == PT_TLS)
			{
				s_dlerror_string = "TODO: __dlopen with TLS";
				return nullptr;
			}
		}

		relocate_elf(elf, lazy);
		call_init_funcs(elf, false);
		register_fini_funcs(elf, false);
		syscall(SYS_CLOSE, elf.fd);
	}

	return &elf;
}

void* __dlsym(void* __restrict handle, const char* __restrict name)
{
	if (handle == nullptr)
	{
		for (size_t i = 0; i < s_loaded_file_count; i++)
			if (auto* sym = __dlsym(&s_loaded_files[i], name))
				return sym;
		return nullptr;
	}

	auto& elf = *static_cast<LoadedElf*>(handle);

	// FIXME: look in ELF's dependency tree
	if (auto* match = find_symbol(elf, name))
		return reinterpret_cast<void*>(elf.base + match->st_value);

	s_dlerror_string = "symbol not found";
	return nullptr;
}

static bool elf_contains_address(const LoadedElf& elf, const void* address)
{
	const uintptr_t addr_uptr = reinterpret_cast<uintptr_t>(address);
	for (size_t i = 0; i < elf.loaded_phdr_count; i++)
	{
		const auto& phdr = elf.loaded_phdrs[i];
		if (phdr.base <= addr_uptr && addr_uptr < phdr.base + phdr.size)
			return true;
	}
	return false;
}

struct FindSymbolResult
{
	const char* name;
	void* addr;
};

static FindSymbolResult find_symbol_containing(const LoadedElf& elf, const void* address)
{
	const uintptr_t addr_uptr = reinterpret_cast<uintptr_t>(address);

	const size_t symbol_count = reinterpret_cast<const uint32_t*>(elf.hash)[1];
	for (size_t i = 1; i < symbol_count; i++)
	{
		const auto& symbol = *reinterpret_cast<const ElfNativeSymbol*>(elf.symtab + i * elf.syment);
		const uintptr_t symbol_base = elf.base + symbol.st_value;
		if (!(symbol_base <= addr_uptr && addr_uptr < symbol_base + symbol.st_size))
			continue;
		return {
			.name = reinterpret_cast<const char*>(elf.strtab + symbol.st_name),
			.addr = reinterpret_cast<void*>(symbol_base),
		};
	}

	if (!elf.real_symtab_addr || !elf.real_strtab_addr)
		return {};

	for (size_t i = 1; i < elf.real_symtab_size / elf.real_symtab_entsize; i++)
	{
		const auto& symbol = *reinterpret_cast<const ElfNativeSymbol*>(elf.real_symtab_addr + i * elf.real_symtab_entsize);
		const uintptr_t symbol_base = elf.base + symbol.st_value;
		if (!(symbol_base <= addr_uptr && addr_uptr < symbol_base + symbol.st_size))
			continue;
		return {
			.name = reinterpret_cast<const char*>(elf.real_strtab_addr + symbol.st_name),
			.addr = reinterpret_cast<void*>(symbol_base),
		};
	}

	return {};
}

int __dladdr(const void* addr, Dl_info_t* dlip)
{
	for (size_t i = 0; i < s_loaded_file_count; i++)
	{
		const auto& elf = s_loaded_files[i];
		if (!elf_contains_address(elf, addr))
			continue;

		dlip->dli_fname = elf.path;
		dlip->dli_fbase = reinterpret_cast<void*>(elf.base);

		if (const auto symbol = find_symbol_containing(elf, addr); symbol.addr && symbol.name)
		{
			dlip->dli_sname = symbol.name;
			dlip->dli_saddr = symbol.addr;
		}
		else
		{
			dlip->dli_sname = nullptr;
			dlip->dli_saddr = nullptr;
		}

		return 1;
	}

	s_dlerror_string = "address is not contained in any file";
	return 0;
}

static LibELF::AuxiliaryVector* find_auxv(char** envp)
{
	if (envp == nullptr)
		return nullptr;

	char** null_env = envp;
	while (*null_env)
		null_env++;

	return reinterpret_cast<LibELF::AuxiliaryVector*>(null_env + 1);
}

static bool starts_with(const char* string, const char* comp)
{
	size_t i = 0;
	for (; string[i] && comp[i]; i++)
		if (string[i] != comp[i])
			return false;
	return comp[i] == '\0';
}

extern "C"
__attribute__((used))
uintptr_t _entry(int argc, char* argv[], char* envp[])
{
	for (size_t i = 0; envp[i]; i++)
		if (starts_with(envp[i], "LD_LIBRARY_PATH="))
			s_ld_library_path = envp[i] + 16;

	int execfd = -1;
	if (auto* auxv = find_auxv(envp))
		for (auto* aux = auxv; aux->a_type != LibELF::AT_NULL; aux++)
			if (aux->a_type == LibELF::AT_EXECFD) {
				execfd = aux->a_un.a_val;
				aux->a_type = LibELF::AT_IGNORE;
			}

	if (execfd == -1)
	{
		if (argc < 2)
			print_error_and_exit("missing program name", 0);

		argc--;
		argv++;

		execfd = syscall(SYS_OPENAT, AT_FDCWD, argv[0], O_RDONLY);
		if (execfd < 0)
			print_error_and_exit("could not open program", execfd);
	}

	init_random();
	auto& elf = load_elf(argv[0], execfd);
	fini_random();

	const auto master_tls = initialize_master_tls();
	relocate_elf(elf, true);
	initialize_tls(master_tls);
	initialize_environ(envp);
	call_init_funcs(elf, true);
	register_fini_funcs(elf, true);

	for (size_t i = 0; i < s_loaded_file_count; i++)
		syscall(SYS_CLOSE, s_loaded_files[i].fd);

	return elf.base + elf.file_header.e_entry;
}
