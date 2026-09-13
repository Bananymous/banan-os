#include <BAN/Atomic.h>
#include <BAN/HashMap.h>
#include <BAN/Optional.h>
#include <BAN/String.h>

#include <alloca.h>
#include <dlfcn.h>
#include <elf.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/random.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <unistd.h>

#if defined(__x86_64__)
#define ELF_NATIVE_TYPE(name) using Elf_ ## name = Elf64_ ## name;
#define ELF_R_SYM   ELF64_R_SYM
#define ELF_R_TYPE  ELF64_R_TYPE
#define ELF_ST_BIND ELF64_ST_BIND
#define ELF_ST_TYPE ELF64_ST_TYPE
#elif defined(__i686__)
#define ELF_NATIVE_TYPE(name) using Elf_ ## name = Elf32_ ## name;
#define ELF_R_SYM   ELF32_R_SYM
#define ELF_R_TYPE  ELF32_R_TYPE
#define ELF_ST_BIND ELF32_ST_BIND
#define ELF_ST_TYPE ELF32_ST_TYPE
#endif

#define ELF_TYPE_LIST(X) X(Ehdr) X(Shdr) X(Phdr) X(Sym) X(Dyn) X(Rel) X(RelA)
ELF_TYPE_LIST(ELF_NATIVE_TYPE);
#undef ELF_NATIVE_TYPE
#undef ELF_TYPE_LIST

#pragma GCC diagnostic ignored "-Wstack-usage="
#pragma GCC optimize "no-tree-loop-distribute-patterns"

struct DynamicInfo
{
	uintptr_t pltgot;

	uintptr_t hash;

	uintptr_t strtab;

	uintptr_t symtab;
	uint32_t syment;

	uintptr_t rpath;
	uintptr_t runpath;

	uintptr_t jmprel;
	uint32_t pltrelsz;
	uint32_t pltrel;

	uintptr_t rela;
	uint32_t relasz;
	uint32_t relaent;

	uintptr_t rel;
	uint32_t relsz;
	uint32_t relent;

	uintptr_t init;
	uintptr_t init_array;
	uint32_t init_arraysz;

	uintptr_t fini;
	uintptr_t fini_array;
	uint32_t fini_arraysz;

	bool symbolic;
	bool textrel;
};

struct LoadedObject
{
	const char* full_path  { nullptr };
	uintptr_t base_address { 0 };
	uintptr_t entry_point  { 0 };

	bool is_local        { false };
	bool relocated       { false };
	bool called_init     { false };
	bool registered_fini { false };
	bool has_loaded_tls  { false };

	Elf_Ehdr file_header { };
	BAN::Vector<Elf_Phdr> program_headers;

	size_t tls_module { 0 };
	size_t tls_offset { 0 };
	BAN::Optional<Elf_Phdr> tls_header;

	DynamicInfo dynamic { };

	BAN::Vector<LoadedObject*> scope_roots;
	BAN::Vector<LoadedObject*> lookup_scope;
	BAN::Vector<LoadedObject*> dependencies;

	bool checked_symbol_table { false };
	const uint8_t* symtab { nullptr };
	const uint8_t* strtab { nullptr };
	size_t numsyms { 0 };
	size_t syment { 0 };
};

static LoadedObject s_self {};

static size_t s_next_tls_module { 1 };
static _dynamic_tls_t* s_dynamic_tls { nullptr };
static constexpr size_t s_max_tls_modules { sizeof(uthread::dtv) / sizeof(*uthread::dtv) - 1 };

static LoadedObject* s_global_scope { nullptr };
static BAN::HashMap<BAN::String, LoadedObject*> s_loaded_objects;
static const char* s_ld_library_path { nullptr };
static const char* s_ld_preload { nullptr };

template<typename T> concept Elf_Rel_c = BAN::is_same_v<T, Elf_Rel> || BAN::is_same_v<T, Elf_RelA>;

extern "C"
{
	void _libc_entry();
	static uintptr_t _libc_main(int argc, char* argv[], char* envp[]);
	static uintptr_t _resolve_symbol(const LoadedObject& info, uintptr_t plt_entry);
	static void _resolve_symbol_trampoline();
}

struct ScopedGlobalLock
{
	ScopedGlobalLock()
	{
		pthread_mutex_lock(&m_mutex);
	}

	~ScopedGlobalLock()
	{
		pthread_mutex_unlock(&m_mutex);
	}

private:
	static pthread_mutex_t m_mutex;
};
pthread_mutex_t ScopedGlobalLock::m_mutex { PTHREAD_MUTEX_INITIALIZER };

static uint32_t elf_hash(BAN::StringView name)
{
	uint32_t h = 0, g;
	for (const char ch : name)
	{
		h = (h << 4) + ch;
		if ((g = h & 0xF0000000))
			h ^= g >> 24;
		h &= ~g;
	}
	return h;
}

static Elf_Sym* find_symbol_in_object(const LoadedObject& object, BAN::StringView target_name)
{
	const uint32_t* hash_table = reinterpret_cast<uint32_t*>(object.dynamic.hash);
	const uint32_t nbucket = hash_table[0];

	for (uint32_t entry = hash_table[2 + (elf_hash(target_name) % nbucket)]; entry; entry = hash_table[2 + nbucket + entry])
	{
		auto& symbol = *reinterpret_cast<Elf_Sym*>(object.dynamic.symtab + entry * object.dynamic.syment);
		if (symbol.st_shndx == 0)
			continue;
		BAN::StringView symbol_name = reinterpret_cast<const char*>(object.dynamic.strtab + symbol.st_name);
		if (symbol_name != target_name)
			continue;
		return &symbol;
	}

	return nullptr;
}

struct FindSymbolInScopeResult
{
	const LoadedObject* object;
	uintptr_t address;
};

static FindSymbolInScopeResult find_symbol_in_scope(const LoadedObject& object, BAN::StringView symbol_name)
{
	if (object.scope_roots.empty())
	{
		const auto* match = find_symbol_in_object(object, symbol_name);
		if (match == nullptr)
			return { nullptr, 0 };
		if (match->st_value == 0)
			return { &object, 0 };
		return { &object, object.base_address + match->st_value };
	}

	FindSymbolInScopeResult weak = { nullptr , 0 };

	if (object.dynamic.symbolic)
	{
		if (const auto* match = find_symbol_in_object(object, symbol_name))
		{
			if (ELF_ST_BIND(match->st_info) != STB_WEAK)
				return { &object, object.base_address + match->st_value };
			if (weak.object == nullptr)
				weak = { &object, match->st_value ? object.base_address + match->st_value : 0 };
		}
	}

	for (const auto& scope_root : object.scope_roots)
	{
		for (const auto* lookup : scope_root->lookup_scope)
		{
			const auto* match = find_symbol_in_object(*lookup, symbol_name);
			if (match == nullptr || (ELF_ST_BIND(match->st_info) == STB_LOCAL && lookup != &object))
				continue;
			if (ELF_ST_BIND(match->st_info) != STB_WEAK)
				return { lookup, lookup->base_address + match->st_value };
			if (weak.object == nullptr)
				weak = { lookup, match->st_value ? lookup->base_address + match->st_value : 0 };
		}
	}

	return weak;
}

static FindSymbolInScopeResult find_symbol_in_scope(const LoadedObject& object, uint32_t symbol_index)
{
	const auto& symbol = *reinterpret_cast<Elf_Sym*>(object.dynamic.symtab + symbol_index * object.dynamic.syment);
	if (ELF_ST_BIND(symbol.st_info) == STB_LOCAL && symbol.st_shndx)
		return { &object, object.base_address + symbol.st_value };

	const char* symbol_name = reinterpret_cast<const char*>(object.dynamic.strtab + symbol.st_name);
	return find_symbol_in_scope(object, symbol_name);
}

template<Elf_Rel_c RelocT>
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

template<Elf_Rel_c RelocT>
static void handle_copy_relocation(const LoadedObject& object, const RelocT& reloc)
{
	if (!is_copy_relocation(reloc))
		return;

	const uint32_t symbol_index = ELF_R_SYM(reloc.r_info);
	if (symbol_index == 0)
	{
		fprintf(stderr, "copy relocation without a symbol\n");
		exit(1);
	}

	const auto& symbol = *reinterpret_cast<Elf_Sym*>(object.dynamic.symtab + symbol_index * object.dynamic.syment);
	const char* symbol_name = reinterpret_cast<const char*>(object.dynamic.strtab + symbol.st_name);

	uintptr_t src_address = 0;
	for (size_t i = 1; i < object.lookup_scope.size(); i++)
	{
		const auto& lookup = *object.lookup_scope[i];
		const auto* match = find_symbol_in_object(lookup, symbol_name);
		if (match == nullptr || ELF_ST_BIND(match->st_info) == STB_LOCAL)
			continue;
		if (ELF_ST_BIND(match->st_info) != STB_WEAK || src_address == 0)
			src_address = lookup.base_address + match->st_value;
		if (ELF_ST_BIND(match->st_info) != STB_WEAK)
			break;
	}

	if (src_address == 0)
	{
		fprintf(stderr, "copy relocation source not found\n");
		exit(1);
	}

	memcpy(
		reinterpret_cast<void*>(object.base_address + reloc.r_offset),
		reinterpret_cast<void*>(src_address),
		symbol.st_size
	);
}

template<Elf_Rel_c RelocT>
static bool is_tls_relocation(const RelocT& reloc)
{
	switch (ELF_R_TYPE(reloc.r_info))
	{
#if defined(__x86_64__)
		case R_X86_64_DTPMOD64:
		case R_X86_64_DTPOFF64:
		case R_X86_64_TPOFF64:
		case R_X86_64_TLSGD:
		case R_X86_64_TLSLD:
		case R_X86_64_DTPOFF32:
		case R_X86_64_GOTTPOFF:
		case R_X86_64_TPOFF32:
			return true;
#elif defined(__i686__)
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
#else
	#error "unsupported architecture"
#endif
	}
	return false;
}

template<Elf_Rel_c RelocT>
static void handle_tls_relocation(const LoadedObject& object, const RelocT& reloc)
{
	if (!is_tls_relocation(reloc))
		return;

	auto found_symbol = FindSymbolInScopeResult { &object, 0 };
	if (const uint32_t symbol_index = ELF_R_SYM(reloc.r_info))
		found_symbol = find_symbol_in_scope(object, symbol_index);

	if (found_symbol.object == nullptr)
	{
		fprintf(stderr, "no object for TLS relocation??\n");
		exit(1);
	}

	if (found_symbol.address)
		found_symbol.address -= found_symbol.object->base_address;

	switch (ELF_R_TYPE(reloc.r_info))
	{
#if defined(__x86_64__)
		case R_X86_64_DTPMOD64:
			*reinterpret_cast<uint64_t*>(object.base_address + reloc.r_offset) = found_symbol.object->tls_module;
			break;
		case R_X86_64_DTPOFF64:
			*reinterpret_cast<uint64_t*>(object.base_address + reloc.r_offset) = found_symbol.address;
			break;
		case R_X86_64_TPOFF64:
			*reinterpret_cast<uint64_t*>(object.base_address + reloc.r_offset) = found_symbol.address - found_symbol.object->tls_offset;
			break;
#elif defined(__i686__)
		case R_386_TLS_DTPMOD32:
			*reinterpret_cast<uint32_t*>(object.base_address + reloc.r_offset) = found_symbol.object->tls_module;
			break;
		case R_386_TLS_DTPOFF32:
			*reinterpret_cast<uint32_t*>(object.base_address + reloc.r_offset) = found_symbol.address;
			break;
		case R_386_TLS_TPOFF:
			*reinterpret_cast<uint32_t*>(object.base_address + reloc.r_offset) = found_symbol.address - found_symbol.object->tls_offset;
			break;
#endif
		default:
			fprintf(stderr, "unsupported TLS relocation type %u in %s\n", static_cast<unsigned>(ELF_R_TYPE(reloc.r_info)), object.full_path);
			exit(1);
	}
}

template<Elf_Rel_c RelocT>
static uintptr_t handle_relocation(const LoadedObject& object, const RelocT& reloc, bool resolve_symbols)
{
	if (is_copy_relocation(reloc) || is_tls_relocation(reloc))
		return 0;

	const uint32_t symbol_index = ELF_R_SYM(reloc.r_info);
	if (resolve_symbols == !symbol_index)
		return 0;

	uintptr_t symbol_address = 0;
	if (symbol_index != 0)
	{
		const auto& symbol = *reinterpret_cast<Elf_Sym*>(object.dynamic.symtab + symbol_index * object.dynamic.syment);
		const auto result = find_symbol_in_scope(object, symbol_index);
		if (result.object == nullptr && ELF_ST_BIND(symbol.st_info) != STB_WEAK)
		{
			// FIXME: propagate errors, this is non fatal for dlopen
			const char* symbol_name = reinterpret_cast<const char*>(object.dynamic.strtab + symbol.st_name);
			fprintf(stderr, "symbol '%s' not found\n", symbol_name);
			_exit(1);
		}

		symbol_address = result.address;
	}

	size_t size = 0;
	uintptr_t value = 0;
	bool add_addend = false;

	switch (ELF_R_TYPE(reloc.r_info))
	{
#if defined(__x86_64__)
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
			value = object.base_address;
			add_addend = true;
			break;
#elif defined(__i686__)
		case R_386_NONE:
			break;
		case R_386_32:
			size = 4;
			value = symbol_address;
			add_addend = true;
			break;
		case R_386_PC32:
			size = 4;
			value = symbol_address - (object.base_address + reloc.r_offset);
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
			value = object.base_address;
			add_addend = true;
			break;
#else
	#error "unsupported architecture"
#endif
		default:
			fprintf(stderr, "unsupported relocation type %u in %s\n", static_cast<unsigned>(ELF_R_TYPE(reloc.r_info)), object.full_path);
			exit(1);
	}

	if (add_addend)
	{
		if constexpr(BAN::is_same_v<RelocT, Elf_RelA>)
			value += reloc.r_addend;
		else
		{
			switch (size)
			{
				case 0: break;
				case 1: value += *reinterpret_cast<uint8_t*> (object.base_address + reloc.r_offset); break;
				case 2: value += *reinterpret_cast<uint16_t*>(object.base_address + reloc.r_offset); break;
				case 4: value += *reinterpret_cast<uint32_t*>(object.base_address + reloc.r_offset); break;
				case 8: value += *reinterpret_cast<uint64_t*>(object.base_address + reloc.r_offset); break;
			}
		}
	}

	switch (size)
	{
		case 0: break;
		case 1: *reinterpret_cast<uint8_t*> (object.base_address + reloc.r_offset) = value; break;
		case 2: *reinterpret_cast<uint16_t*>(object.base_address + reloc.r_offset) = value; break;
		case 4: *reinterpret_cast<uint32_t*>(object.base_address + reloc.r_offset) = value; break;
		case 8: *reinterpret_cast<uint64_t*>(object.base_address + reloc.r_offset) = value; break;
	}

	return value;
}

static void parse_dynamic_info(LoadedObject& object, BAN::Span<const Elf_Dyn> dynamics)
{
	auto& info = object.dynamic;

	const uintptr_t base = object.base_address;
	for (const auto& dynamic : dynamics)
	{
		ASSERT(dynamic.d_tag != DT_NULL);
		switch (dynamic.d_tag)
		{
			case DT_PLTGOT:       info.pltgot       = dynamic.d_un.d_ptr + base; break;
			case DT_HASH:         info.hash         = dynamic.d_un.d_ptr + base; break;
			case DT_STRTAB:       info.strtab       = dynamic.d_un.d_ptr + base; break;
			case DT_SYMTAB:       info.symtab       = dynamic.d_un.d_ptr + base; break;
			case DT_SYMENT:       info.syment       = dynamic.d_un.d_val;        break;
			case DT_JMPREL:       info.jmprel       = dynamic.d_un.d_ptr + base; break;
			case DT_PLTRELSZ:     info.pltrelsz     = dynamic.d_un.d_val;        break;
			case DT_PLTREL:       info.pltrel       = dynamic.d_un.d_val;        break;
			case DT_RELA:         info.rela         = dynamic.d_un.d_ptr + base; break;
			case DT_RELASZ:       info.relasz       = dynamic.d_un.d_val;        break;
			case DT_RELAENT:      info.relaent      = dynamic.d_un.d_val;        break;
			case DT_REL:          info.rel          = dynamic.d_un.d_ptr + base; break;
			case DT_RELSZ:        info.relsz        = dynamic.d_un.d_val;        break;
			case DT_RELENT:       info.relent       = dynamic.d_un.d_val;        break;
			case DT_INIT:         info.init         = dynamic.d_un.d_ptr + base; break;
			case DT_INIT_ARRAY:   info.init_array   = dynamic.d_un.d_ptr + base; break;
			case DT_INIT_ARRAYSZ: info.init_arraysz = dynamic.d_un.d_val;        break;
			case DT_FINI:         info.fini         = dynamic.d_un.d_ptr + base; break;
			case DT_FINI_ARRAY:   info.fini_array   = dynamic.d_un.d_ptr + base; break;
			case DT_FINI_ARRAYSZ: info.fini_arraysz = dynamic.d_un.d_val;        break;
			case DT_RPATH:        info.rpath        = dynamic.d_un.d_val;        break;
			case DT_RUNPATH:      info.runpath      = dynamic.d_un.d_val;        break;
			case DT_SYMBOLIC:     info.symbolic     = true;                      break;
			case DT_TEXTREL:      info.textrel      = true;                      break;
		}
	}
}

__attribute__((used))
static uintptr_t _resolve_symbol(const LoadedObject& object, uintptr_t plt_entry)
{
	ScopedGlobalLock _;

	uintptr_t result;
	switch (object.dynamic.pltrel)
	{
		case DT_REL:
			result = handle_relocation(object, *reinterpret_cast<Elf_Rel*>(object.dynamic.jmprel + plt_entry), true);
			break;
		case DT_RELA:
			result = handle_relocation(object, reinterpret_cast<Elf_RelA*>(object.dynamic.jmprel)[plt_entry], true);
			break;
		default:
			fprintf(stderr, "pltrel %u in '%s'\n", object.dynamic.pltrel, object.full_path);
			exit(1);
	}

	return result;
}

// FIXME: Don't map read-only sections as writable with DT_TEXTREL.
//        Instead mprotect the areas writable during relocation.
static bool load_program_header(const Elf_Phdr& program_header, int fd, bool has_textrel)
{
	const int prot = ({
		int prot = 0;
		if (program_header.p_flags & PF_R)
			prot |= PROT_READ;
		if (program_header.p_flags & PF_W)
			prot |= PROT_WRITE;
		if (program_header.p_flags & PF_X)
			prot |= PROT_EXEC;
		prot;
	});

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
		void* mmap_ret = mmap(
			reinterpret_cast<void*>(program_header.p_vaddr),
			file_backed_size,
			prot | (has_textrel ? PROT_WRITE : 0),
			MAP_PRIVATE | MAP_FIXED_NOREPLACE,
			fd,
			program_header.p_offset
		);
		if (mmap_ret == MAP_FAILED)
			return false;
	}

	if (file_backed_size < program_header.p_memsz)
	{
		const uintptr_t mmap_addr = (program_header.p_vaddr + file_backed_size) & ~(uintptr_t)0xFFF;
		const size_t mmap_length = (program_header.p_vaddr - mmap_addr) + program_header.p_memsz;

		void* mmap_ret = mmap(
			reinterpret_cast<void*>(mmap_addr),
			mmap_length,
			prot | PROT_WRITE,
			MAP_ANONYMOUS | MAP_PRIVATE | MAP_FIXED_NOREPLACE,
			-1,
			0
		);
		if (mmap_ret == MAP_FAILED)
			return false;

		if (file_backed_size < program_header.p_filesz)
		{
			const uintptr_t addr = program_header.p_vaddr + file_backed_size;
			const uintptr_t size = program_header.p_filesz - file_backed_size;
			const size_t offset = program_header.p_offset + file_backed_size;
			if (pread(fd, reinterpret_cast<void*>(addr), size, offset) != static_cast<long>(size))
				return false;
		}

		if (!(prot & PROT_WRITE) && !has_textrel)
			if (mprotect(reinterpret_cast<void*>(mmap_addr), mmap_length, prot) == -1)
				return false;
	}

	return true;
}

static bool can_load_object(BAN::Span<const Elf_Phdr> program_headers, uintptr_t base)
{
	// FIXME: This does not guarantee the mapping succeeds after this does.
	//        Another thread could be doing mmaps that end up overlapping.

	for (const auto& program_header : program_headers)
	{
		if (program_header.p_type != PT_LOAD)
			continue;

		const uintptr_t absolute_addr = program_header.p_vaddr + base;
		const uintptr_t mmap_addr = absolute_addr & ~(uintptr_t)0xFFF;
		const size_t mmap_length = (absolute_addr - mmap_addr) + program_header.p_memsz;

		void* mmap_ret = mmap(
			reinterpret_cast<void*>(mmap_addr),
			mmap_length,
			PROT_NONE,
			MAP_ANONYMOUS | MAP_PRIVATE | MAP_FIXED_NOREPLACE,
			-1,
			0
		);
		if (mmap_ret == MAP_FAILED)
			return false;

		munmap(mmap_ret, mmap_length);
	}

	return true;
}

static bool validate_file_header(const Elf_Ehdr& file_header)
{
	if (file_header.e_ident[EI_MAG0] != ELFMAG0 ||
		file_header.e_ident[EI_MAG1] != ELFMAG1 ||
		file_header.e_ident[EI_MAG2] != ELFMAG2 ||
		file_header.e_ident[EI_MAG3] != ELFMAG3)
	{
		return false;
	}

	if (file_header.e_ident[EI_DATA] != ELFDATA2LSB)
		return false;

	if (file_header.e_ident[EI_VERSION] != EV_CURRENT)
		return false;

#if defined(__x86_64__)
	if (file_header.e_ident[EI_CLASS] != ELFCLASS64)
#elif defined(__i686__)
	if (file_header.e_ident[EI_CLASS] != ELFCLASS32)
#else
	#error "unsupported architecture"
#endif
		return false;

	if (file_header.e_version != EV_CURRENT)
		return false;

	return true;
}

static char* find_library(const LoadedObject& object, BAN::StringView needed_name)
{
	if (needed_name.contains('/'))
	{
		char* buffer = static_cast<char*>(alloca(needed_name.size() + 1));
		memcpy(buffer, needed_name.data(), needed_name.size());
		buffer[needed_name.size()] = '\0';
		return realpath(buffer, nullptr);
	}

	const auto check_library = [&](BAN::StringView library_dir) -> char* {
		char* buffer = static_cast<char*>(alloca(library_dir.size() + 1 + needed_name.size() + 1));

		char* ptr = buffer;
		const auto append_buffer = [&ptr](const char* string, size_t size) {
			memcpy(ptr, string, size);
			ptr += size;
		};

		append_buffer(library_dir.data(), library_dir.size());
		append_buffer("/",  1);
		append_buffer(needed_name.data(), needed_name.size());
		append_buffer("\0", 1);

		// FIXME: this should do DT_SONAME lookup, maybe a dynamic loader cache?
		return realpath(buffer, nullptr);
	};

	const auto check_library_list = [&](const char* library_list) -> char* {
		for (const char* ptr = library_list;; ptr++)
		{
			if (*ptr != ':' && *ptr != '\0')
				continue;
			if (char* result = check_library({ library_list, static_cast<size_t>(ptr - library_list) }))
				return result;
			if (*ptr == '\0')
				break;
			library_list = ptr + 1;
		}
		return nullptr;
	};

	if (object.dynamic.rpath && !object.dynamic.runpath)
		if (char* result = check_library_list(reinterpret_cast<const char*>(object.dynamic.strtab + object.dynamic.rpath)))
			return result;

	if (s_ld_library_path)
		if (char* result = check_library_list(s_ld_library_path))
			return result;

	if (object.dynamic.runpath)
		if (char* result = check_library_list(reinterpret_cast<const char*>(object.dynamic.strtab + object.dynamic.runpath)))
			return result;

	return check_library_list("/lib:/usr/lib");
}

static void promote_to_global(LoadedObject& object)
{
	if (!object.is_local)
		return;
	object.is_local = false;

	ASSERT(!s_global_scope->lookup_scope.contains(&object));
	MUST(s_global_scope->lookup_scope.push_back(&object));

	for (auto* dependency : object.dependencies)
		promote_to_global(*dependency);
}

// NOTE: this takes ownership of full_path
static LoadedObject* load_object(char* full_path, int fd, bool load_local)
{
	struct path_deleter {
		~path_deleter() { if (to_free) free(to_free); }
		char* to_free;
	} path_deleter { full_path };

	if (auto it = s_loaded_objects.find(BAN::StringView { full_path }); it != s_loaded_objects.end())
	{
		if (it->value->is_local && !load_local)
			promote_to_global(*it->value);
		return it->value;
	}

	if (strcmp(full_path, s_self.full_path) == 0)
	{
		auto* object = static_cast<LoadedObject*>(malloc(sizeof(LoadedObject)));
		if (object == nullptr)
			return nullptr;
		new (object) LoadedObject();

		if (s_loaded_objects.insert(BAN::String(s_self.full_path), object).is_error())
		{
			object->~LoadedObject();
			free(object);
			return nullptr;
		}

		*object = BAN::move(s_self);
		object->full_path = full_path;
		if (uintptr_t* pltgot = reinterpret_cast<uintptr_t*>(object->dynamic.pltgot))
			pltgot[1] = reinterpret_cast<uintptr_t>(&object);

		path_deleter.to_free = nullptr;

		return object;
	}

	const bool fd_needs_close = (fd == -1);
	if (fd == -1 && (fd = open(full_path, O_RDONLY)) == -1)
		return nullptr;

	Elf_Ehdr file_header;
	if (pread(fd, &file_header, sizeof(file_header), 0) != sizeof(file_header))
		return nullptr;
	if (!validate_file_header(file_header))
		return nullptr;

	BAN::Vector<Elf_Phdr> program_headers;
	MUST(program_headers.resize(file_header.e_phnum));
	for (size_t i = 0, offset = file_header.e_phoff; i < program_headers.size(); i++, offset += file_header.e_phentsize)
	{
		if (pread(fd, &program_headers[i], sizeof(Elf_Phdr), offset) != sizeof(Elf_Phdr))
			return nullptr;
		if (program_headers[i].p_memsz < program_headers[i].p_filesz)
			return nullptr;
	}

	uintptr_t base_address;
	if (file_header.e_type == ET_EXEC)
	{
		base_address = 0;
		if (!can_load_object(program_headers.span(), base_address))
			return nullptr;
	}
	else if (file_header.e_type == ET_DYN)
	{
		do {
			if (getrandom(&base_address, sizeof(base_address), 0) != sizeof(base_address))
			{
				fprintf(stderr, "could not generate a random number: %m\n");
				return nullptr;
			}
#if defined(__x86_64__)
		base_address &= 0x7FFFFFFFF000;
#elif defined(__i686__)
		base_address &= 0x7FFFF000;
#else
		#error "unsupported architecture"
#endif
		} while (base_address < 0x100000 || !can_load_object(program_headers.span(), base_address));
	}
	else
	{
		return nullptr;
	}

	for (auto& program_header : program_headers)
		program_header.p_vaddr += base_address;

	uintptr_t dynamic_header_vaddr { 0 };
	BAN::Vector<Elf_Dyn> dynamics;
	for (const auto& program_header : program_headers)
	{
		if (program_header.p_type != PT_DYNAMIC)
			continue;

		if (dynamic_header_vaddr != 0)
			return nullptr;
		dynamic_header_vaddr = program_header.p_vaddr;

		const size_t max_dynamics = program_header.p_memsz / sizeof(Elf_Dyn);
		if (dynamics.resize(max_dynamics).is_error())
			return nullptr;

		const ssize_t dynamics_bytes = dynamics.size() * sizeof(Elf_Dyn);
		if (pread(fd, dynamics.data(), dynamics_bytes, program_header.p_offset) != dynamics_bytes)
			return nullptr;

		for (size_t i = 0; i < dynamics.size(); i++)
		{
			if (dynamics[i].d_tag != DT_NULL)
				continue;
			MUST(dynamics.resize(i));
			break;
		}
	}

	auto* object = static_cast<LoadedObject*>(malloc(sizeof(LoadedObject)));
	if (object == nullptr)
		return nullptr;
	new (object) LoadedObject();

	object->is_local = load_local;
	object->base_address = base_address;
	object->entry_point = base_address + file_header.e_entry;
	object->full_path = full_path;
	parse_dynamic_info(*object, dynamics.span());

	if (s_loaded_objects.insert(BAN::String { full_path }, object).is_error())
	{
		object->~LoadedObject();
		free(object);
		return nullptr;
	}

	path_deleter.to_free = nullptr;

	// FIXME: memory leaks after this point if loading fails (object, mapped headers, dependencies)

	for (const auto& program_header : program_headers)
	{
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
				object->tls_header = program_header;
				object->tls_module = s_next_tls_module++;
				break;
			case PT_LOAD:
				if (!load_program_header(program_header, fd, object->dynamic.textrel))
					return nullptr;
				break;
			default:
				fprintf(stderr, "unsupported program header type %x\n", program_header.p_type);
				return nullptr;
		}
	}

	object->file_header = file_header;
	object->program_headers = BAN::move(program_headers);

	// do relocations without symbols
	if (object->dynamic.rel && object->dynamic.relent)
		for (size_t i = 0; i < object->dynamic.relsz / object->dynamic.relent; i++)
			handle_relocation(*object, *reinterpret_cast<Elf_Rel*>(object->dynamic.rel + i * object->dynamic.relent), false);
	if (object->dynamic.rela && object->dynamic.relaent)
		for (size_t i = 0; i < object->dynamic.relasz / object->dynamic.relaent; i++)
			handle_relocation(*object, *reinterpret_cast<Elf_RelA*>(object->dynamic.rela + i * object->dynamic.relaent), false);

	// setup required GOT entries
	if (uintptr_t* pltgot = reinterpret_cast<uintptr_t*>(object->dynamic.pltgot))
	{
		pltgot[0] = dynamic_header_vaddr;
		pltgot[1] = reinterpret_cast<uintptr_t>(object);
		pltgot[2] = reinterpret_cast<uintptr_t>(_resolve_symbol_trampoline);
	}

	const auto load_dependency = [object, load_local](BAN::StringView name) -> bool {
		if (char* full_path = find_library(*object, name); full_path == nullptr)
			return false;
		else if (auto* dependency = load_object(full_path, -1, load_local); dependency == nullptr)
			return false;
		else if (object->dependencies.push_back(dependency).is_error())
			return false;
		return true;
	};

	const char* preload = s_ld_preload;
	s_ld_preload = nullptr;

	for (const char* ptr = preload; preload; ptr++)
	{
		if (*ptr != ':' && *ptr != ' ' && *ptr != '\0')
			continue;
		if (!load_dependency({ preload, static_cast<size_t>(ptr - preload) }))
			fprintf(stderr, "could not preload library %*s\n", static_cast<int>(ptr - preload), preload);
		if (*ptr == '\0')
			break;
		preload = ptr + 1;
	}

	for (const auto& dynamic : dynamics)
	{
		if (dynamic.d_tag != DT_NEEDED)
			continue;
		if (!load_dependency(reinterpret_cast<const char*>(object->dynamic.strtab + dynamic.d_un.d_val)))
			return nullptr;
	}

	if (fd_needs_close)
		close(fd);

	return object;
}

static void relocate_object(LoadedObject& object, bool bind_now)
{
	if (object.relocated)
		return;
	object.relocated = true;

	// relocate libraries
	for (auto* dependency : object.dependencies)
		relocate_object(*dependency, bind_now);

	// do mandatory relocations
	if (object.dynamic.rel && object.dynamic.relent)
	{
		for (size_t i = 0; i < object.dynamic.relsz / object.dynamic.relent; i++)
			handle_relocation(object, *reinterpret_cast<Elf_Rel*>(object.dynamic.rel + i * object.dynamic.relent), true);
		for (size_t i = 0; i < object.dynamic.relsz / object.dynamic.relent; i++)
			handle_tls_relocation(object, *reinterpret_cast<Elf_Rel*>(object.dynamic.rel + i * object.dynamic.relent));
	}
	if (object.dynamic.rela && object.dynamic.relaent)
	{
		for (size_t i = 0; i < object.dynamic.relasz / object.dynamic.relaent; i++)
			handle_relocation(object, *reinterpret_cast<Elf_RelA*>(object.dynamic.rela + i * object.dynamic.relaent), true);
		for (size_t i = 0; i < object.dynamic.relasz / object.dynamic.relaent; i++)
			handle_tls_relocation(object, *reinterpret_cast<Elf_RelA*>(object.dynamic.rela + i * object.dynamic.relaent));
	}

	// do jumprel relocations
	if (object.dynamic.jmprel && object.dynamic.pltrelsz)
	{
		if (bind_now)
		{
			switch (object.dynamic.pltrel)
			{
				case DT_REL:
					for (size_t i = 0; i < object.dynamic.pltrelsz / sizeof(Elf_Rel); i++)
						handle_relocation(object, reinterpret_cast<Elf_Rel*>(object.dynamic.jmprel)[i], true);
					break;
				case DT_RELA:
					for (size_t i = 0; i < object.dynamic.pltrelsz / sizeof(Elf_RelA); i++)
						handle_relocation(object, reinterpret_cast<Elf_RelA*>(object.dynamic.jmprel)[i], true);
					break;
				default:
					ASSERT_NOT_REACHED();
			}
		}
		else
		{
			switch (object.dynamic.pltrel)
			{
				case DT_REL:
					for (size_t i = 0; i < object.dynamic.pltrelsz / sizeof(Elf_Rel); i++)
						*reinterpret_cast<uintptr_t*>(object.base_address + reinterpret_cast<Elf_Rel*>(object.dynamic.jmprel)[i].r_offset) += object.base_address;
					break;
				case DT_RELA:
					for (size_t i = 0; i < object.dynamic.pltrelsz / sizeof(Elf_RelA); i++)
						*reinterpret_cast<uintptr_t*>(object.base_address + reinterpret_cast<Elf_RelA*>(object.dynamic.jmprel)[i].r_offset) += object.base_address;
					break;
				default:
					ASSERT_NOT_REACHED();
			}
		}
	}
}

static void call_init_funcs(LoadedObject& object, bool is_main_elf, size_t depth = 0)
{
	if (object.called_init)
		return;
	object.called_init = true;

	for (auto* dependency : object.dependencies)
		call_init_funcs(*dependency, false, depth + 1);

	// main executable calls its own init functions in _start
	if (is_main_elf)
		return;

	using init_t = void(*)();
	if (object.dynamic.init)
		reinterpret_cast<init_t>(object.dynamic.init)();
	for (size_t i = 0; i < object.dynamic.init_arraysz / sizeof(init_t); i++)
		reinterpret_cast<init_t*>(object.dynamic.init_array)[i]();
}

static void register_fini_funcs(LoadedObject& object, bool is_main_elf)
{
	if (object.registered_fini)
		return;
	object.registered_fini = true;

	// main executable registers its fini functions in _start
	if (!is_main_elf)
	{
		using fini_t = void(*)();
		for (size_t i = 0; i < object.dynamic.fini_arraysz / sizeof(fini_t); i++)
			atexit(reinterpret_cast<fini_t*>(object.dynamic.fini_array)[i]);
		if (object.dynamic.fini)
			atexit(reinterpret_cast<fini_t>(object.dynamic.fini));
	}

	for (auto* dependency : object.dependencies)
		register_fini_funcs(*dependency, false);
}

static void build_lookup_scope(LoadedObject& root_object)
{
	root_object.lookup_scope.clear();

	BAN::Vector<LoadedObject*> current_layer;
	MUST(current_layer.push_back(&root_object));

	while (!current_layer.empty())
	{
		BAN::Vector<LoadedObject*> next_layer;
		for (auto* object : current_layer)
		{
			MUST(root_object.lookup_scope.push_back(object));
			for (auto* dependency : object->dependencies)
				if (!root_object.lookup_scope.contains(dependency) && !current_layer.contains(dependency) && !next_layer.contains(dependency))
					MUST(next_layer.push_back(dependency));
		}
		current_layer = BAN::move(next_layer);
	}
}

static void add_scope_root(LoadedObject& object, LoadedObject* scope_root)
{
	if (object.scope_roots.contains(scope_root))
		return;

	MUST(object.scope_roots.push_back(scope_root));
	for (auto* dependency : object.dependencies)
		add_scope_root(*dependency, scope_root);
}

static void load_dynamic_master_tls_recursive(LoadedObject& object)
{
	const auto load_dynamic_master_tls = [&object] {
		ASSERT(object.tls_module);

		uint8_t* tls_addr = static_cast<uint8_t*>(malloc(object.tls_header->p_memsz));
		if (tls_addr == nullptr)
		{
			fprintf(stderr, "failed to allocate dynamic TLS: %m\n");
			exit(1);
		}

		memcpy(tls_addr, reinterpret_cast<void*>(object.tls_header->p_vaddr), object.tls_header->p_filesz);
		memset(tls_addr + object.tls_header->p_filesz, 0, object.tls_header->p_memsz - object.tls_header->p_filesz);

		int expected = 0;
		while (!BAN::atomic_compare_exchange(s_dynamic_tls->lock, expected, 1))
		{
			syscall(SYS_YIELD);
			expected = 0;
		}

		s_dynamic_tls->entries[s_dynamic_tls->entry_count++] = {
			.master_addr = tls_addr,
			.master_size = object.tls_header->p_memsz,
		};

		BAN::atomic_store(s_dynamic_tls->lock, 0);
	};

	if (object.has_loaded_tls)
		return;
	object.has_loaded_tls = true;

	if (object.tls_header.has_value())
		load_dynamic_master_tls();
	for (auto* dependency : object.dependencies)
		load_dynamic_master_tls_recursive(*dependency);
}

static LoadedObject* find_object_containing(const void* address)
{
	const uintptr_t address_uptr = reinterpret_cast<uintptr_t>(address);
	for (const auto& [_, loaded_object] : s_loaded_objects)
	{
		if (address_uptr < loaded_object->base_address)
			continue;
		for (const auto& program_header : loaded_object->program_headers)
			if (address_uptr >= program_header.p_vaddr && address_uptr < program_header.p_vaddr + program_header.p_memsz)
				return loaded_object;
	}
	return nullptr;
}

static BAN::Atomic<const char*> s_dlerror_string { nullptr };

char* dlerror(void)
{
	return const_cast<char*>(s_dlerror_string.exchange(nullptr));
}

int dlclose(void* handle)
{
	// TODO: maybe actually close handles? (not required by spec)
	(void)handle;
	return 0;
}

void* dlopen(const char* file, int mode)
{
	if (s_global_scope == nullptr)
	{
		s_dlerror_string = "no global scope, executable is probably statically linked";
		return nullptr;
	}

	const bool load_lazy  = (mode & _RTLD_LAZY_NOW_MASK)     == RTLD_LAZY;
	const bool load_local = (mode & _RTLD_GLOBAL_LOCAL_MASK) == RTLD_LOCAL;

	if (file == nullptr)
		return s_global_scope;

	LoadedObject* source_object = s_global_scope;
	if (auto* return_object = find_object_containing(__builtin_return_address(0)))
		source_object = return_object;

	char* full_path = find_library(*source_object, file);
	if (full_path == nullptr)
	{
		s_dlerror_string = "could not find file";
		return nullptr;
	}

	LoadedObject* object;

	{
		ScopedGlobalLock _;

		if (auto it = s_loaded_objects.find(BAN::StringView { full_path }); it != s_loaded_objects.end())
		{
			if (it->value->is_local && !load_local)
				promote_to_global(*it->value);
			free(full_path);
			return it->value;
		}

		if (mode & RTLD_NOLOAD)
		{
			free(full_path);
			return nullptr;
		}

		if (!(object = load_object(full_path, -1, load_local)))
		{
			s_dlerror_string = "failed to load?";
			return nullptr;
		}

		add_scope_root(*object, s_global_scope);

		build_lookup_scope(*object);
		if (load_local)
			add_scope_root(*object, object);
		else
		{
			for (auto* lookup : object->lookup_scope)
				if (!s_global_scope->lookup_scope.contains(lookup))
					MUST(s_global_scope->lookup_scope.push_back(lookup));
			object->lookup_scope.clear();
		}

		ASSERT(!object->relocated);
		ASSERT(!object->called_init);
		ASSERT(!object->registered_fini);

		relocate_object(*object, !load_lazy);

		load_dynamic_master_tls_recursive(*object);
	}

	call_init_funcs(*object, false);
	register_fini_funcs(*object, false);

	return object;
}

void* dlsym(void* __restrict handle, const char* __restrict name)
{
	if (s_global_scope == nullptr)
	{
		s_dlerror_string = "no global scope, executable is probably statically linked";
		return nullptr;
	}

	ScopedGlobalLock _;

	if (handle == RTLD_DEFAULT)
	{
		const auto result = find_symbol_in_scope(*s_global_scope, name);
		if (result.object != nullptr)
			return reinterpret_cast<void*>(result.address);
		s_dlerror_string = "symbol not found";
		return nullptr;
	}


	void* weak_address { nullptr };
	bool found_weak { false };

	const auto name_sv = BAN::StringView { name };
	const auto check_object = [&weak_address, &found_weak, name_sv](const LoadedObject* object) -> void* {
		const auto* match = find_symbol_in_object(*object, name_sv);
		if (match == nullptr)
			return nullptr;
		if (ELF_ST_BIND(match->st_info) != STB_WEAK)
			return reinterpret_cast<void*>(object->base_address + match->st_value);
		if (!found_weak && match->st_value != 0)
			weak_address = reinterpret_cast<void*>(object->base_address + match->st_value);
		found_weak = true;
		return nullptr;

	};

	if (handle == RTLD_NEXT)
	{
		const auto* object = find_object_containing(__builtin_return_address(0));
		if (object == nullptr)
		{
			s_dlerror_string = "could not determine which object called";
			return nullptr;
		}

		for (const auto* root : object->scope_roots)
		{
			size_t i = 0;
			for (; i < root->lookup_scope.size(); i++)
				if (root->lookup_scope[i] == object)
					break;
			for (i = i + 1; i < root->lookup_scope.size(); i++)
				if (void* result = check_object(root->lookup_scope[i]))
					return result;
		}
	}
	else
	{
		BAN::Vector<const LoadedObject*> current_level, checked;
		MUST(current_level.push_back(static_cast<LoadedObject*>(handle)));

		while (!current_level.empty())
		{
			BAN::Vector<const LoadedObject*> next_level;
			for (const auto* object : current_level)
			{
				if (void* result = check_object(object))
					return result;
				MUST(checked.push_back(object));
				for (const auto* dependency : object->dependencies)
					if (!current_level.contains(dependency) && !next_level.contains(dependency) && !checked.contains(dependency))
						MUST(next_level.push_back(dependency));
			}
			current_level = BAN::move(next_level);
		}
	}

	if (found_weak)
		return reinterpret_cast<void*>(weak_address);

	s_dlerror_string = "symbol not found";
	return nullptr;
}

static bool load_symbol_table(LoadedObject& object)
{
	object.checked_symbol_table = true;

	int fd = open(object.full_path, O_RDONLY);
	if (fd == -1)
		return false;

	struct mmap_addr_off
	{
		uint8_t* address;
		size_t offset;
	};

	const auto read_section_header = [&object, fd](size_t index, Elf_Shdr& header) -> bool {
		return pread(fd, &header, sizeof(Elf_Shdr), object.file_header.e_shoff + index * object.file_header.e_shentsize) == sizeof(Elf_Shdr);
	};

	const auto mmap_section_header = [fd](const Elf_Shdr& header) -> mmap_addr_off {
		const size_t offset = header.sh_offset % PAGE_SIZE;
		uint8_t* address = static_cast<uint8_t*>(mmap(
			nullptr,
			header.sh_size + offset,
			PROT_READ,
			MAP_SHARED,
			fd,
			header.sh_offset - offset
		));
		if (address == MAP_FAILED)
			address = nullptr;
		return { address, offset };
	};

	const auto& file_header = object.file_header;
	for (size_t i = 0; i < file_header.e_shnum; i++)
	{
		Elf_Shdr symtab_header;
		if (!read_section_header(i, symtab_header))
			break;
		if (symtab_header.sh_type != SHT_SYMTAB)
			continue;

		Elf_Shdr strtab_header;
		if (!read_section_header(symtab_header.sh_link, strtab_header))
			break;

		auto [symtab_addr, symtab_off] = mmap_section_header(symtab_header);
		auto [strtab_addr, strtab_off] = mmap_section_header(strtab_header);
		if (symtab_addr == nullptr || strtab_addr == nullptr)
		{
			if (symtab_addr)
				munmap(symtab_addr, symtab_header.sh_size + symtab_off);
			if (strtab_addr)
				munmap(strtab_addr, strtab_header.sh_size + strtab_off);
			break;
		}

		object.symtab = symtab_addr + symtab_off;
		object.strtab = strtab_addr + strtab_off;
		object.numsyms = symtab_header.sh_size / symtab_header.sh_entsize;
		object.syment = symtab_header.sh_entsize;
		break;
	}

	close(fd);
	return object.symtab && object.strtab;
}

struct FindSymbolResult
{
	const char* name;
	void* addr;
};

static FindSymbolResult find_symbol_containing(LoadedObject& object, const void* address)
{
	const uintptr_t addr_uptr = reinterpret_cast<uintptr_t>(address);

	const auto check_symbol_table = [&object, addr_uptr](const uint8_t* symtab, size_t numsyms, size_t syment, const uint8_t* strtab) {
		for (size_t i = 1; i < numsyms; i++)
		{
			const auto& symbol = *reinterpret_cast<const Elf_Sym*>(symtab + i * syment);
			const uintptr_t symbol_base = object.base_address + symbol.st_value;
			if (!(symbol_base <= addr_uptr && addr_uptr < symbol_base + symbol.st_size))
				continue;
			return FindSymbolResult {
				.name = reinterpret_cast<const char*>(strtab + symbol.st_name),
				.addr = reinterpret_cast<void*>(symbol_base),
			};
		}
		return FindSymbolResult { nullptr, nullptr };
	};

	if (object.dynamic.symtab && object.dynamic.strtab)
	{
		const uint8_t* symtab = reinterpret_cast<const uint8_t*>(object.dynamic.symtab);
		const uint8_t* strtab = reinterpret_cast<const uint8_t*>(object.dynamic.strtab);
		const size_t numsyms = reinterpret_cast<const uint32_t*>(object.dynamic.hash)[1];
		if (auto result = check_symbol_table(symtab, numsyms, object.dynamic.syment, strtab); result.addr)
			return result;
	}

	if (object.symtab == nullptr || object.strtab == nullptr)
	{
		if (object.checked_symbol_table)
			return {};
		if (!load_symbol_table(object))
			return {};
	}

	return check_symbol_table(object.symtab, object.numsyms, object.syment, object.strtab);
}

int dladdr(const void* addr, Dl_info_t* dlip)
{
	if (s_global_scope == nullptr)
	{
		s_dlerror_string = "no global scope, executable is probably statically linked";
		return 0;
	}

	ScopedGlobalLock _;

	auto* object = find_object_containing(addr);
	if (object == nullptr)
		return 0;

	if (object->full_path == nullptr || object->full_path[0] == '\0')
		fprintf(stddbg, "%p in something???\n", addr);

	*dlip = {
		.dli_fname = object->full_path,
		.dli_fbase = reinterpret_cast<void*>(object->base_address),
		.dli_sname = nullptr,
		.dli_saddr = nullptr,
	};

	if (const auto symbol = find_symbol_containing(*object, addr); symbol.addr && symbol.name)
	{
		dlip->dli_sname = symbol.name;
		dlip->dli_saddr = symbol.addr;
	}

	return 1;
}

struct MasterTLS
{
	uint8_t* addr;
	size_t size;
	size_t module_count;
};

static MasterTLS initialize_tls_stage1()
{
	constexpr auto round =
		[](size_t a, size_t b) -> size_t
		{
			return b * ((a + b - 1) / b);
		};

	size_t max_align = alignof(uthread);
	size_t tls_m_offset = 0;
	size_t module_count = 0;
	for (const auto* lookup : s_global_scope->lookup_scope)
	{
		if (!lookup->tls_header.has_value())
			continue;

		const auto& tls_header = lookup->tls_header.value();
		max_align = BAN::Math::max<size_t>(max_align, tls_header.p_align);
		tls_m_offset = round(tls_m_offset + tls_header.p_memsz, tls_header.p_align);

		module_count++;
	}

	if (module_count == 0)
		return { .addr = nullptr, .size = 0, .module_count = 0 };

	size_t master_tls_size = tls_m_offset;
	if (auto rem = master_tls_size % max_align)
		master_tls_size += max_align - rem;

	uint8_t* master_tls_addr = static_cast<uint8_t*>(malloc(master_tls_size));
	if (master_tls_addr == MAP_FAILED)
	{
		fprintf(stderr, "failed to allocate master TLS\n");
		exit(1);
	}

	size_t tls_offset = 0;
	for (auto* lookup : s_global_scope->lookup_scope)
	{
		if (!lookup->tls_header.has_value())
			continue;
		const auto& tls_header = lookup->tls_header.value();
		tls_offset = round(tls_offset + tls_header.p_memsz, tls_header.p_align);
		lookup->tls_offset = tls_offset;
	}

	return {
		.addr = master_tls_addr,
		.size = master_tls_size,
		.module_count = module_count,
	};
}

static void initialize_tls_stage2(MasterTLS master_tls)
{
	s_dynamic_tls = static_cast<_dynamic_tls_t*>(malloc(sizeof(_dynamic_tls_t) + s_max_tls_modules * sizeof(_dynamic_tls_entry_t)));
	if (s_dynamic_tls == nullptr)
	{
		fprintf(stderr, "failed to allocate dynamic TLS: %m\n");
		exit(1);
	}
	*s_dynamic_tls = {
		.lock = 0,
		.entry_count = 0,
		.entries = reinterpret_cast<_dynamic_tls_entry_t*>(s_dynamic_tls + 1),
	};

	for (const auto* lookup : s_global_scope->lookup_scope)
	{
		if (!lookup->tls_header.has_value())
			continue;
		const auto& tls_header = lookup->tls_header.value();
		uint8_t* master_addr = master_tls.addr + master_tls.size - lookup->tls_offset;
		memcpy(master_addr, reinterpret_cast<void*>(tls_header.p_vaddr), tls_header.p_filesz);
		memset(master_addr + tls_header.p_filesz, 0, tls_header.p_memsz - tls_header.p_filesz);
	}

	uint8_t* tls_addr = static_cast<uint8_t*>(malloc(master_tls.size + sizeof(uthread)));
	if (tls_addr == nullptr)
	{
		fprintf(stderr, "failed to allocate main thread TLS: %m\n");
		exit(1);
	}

	uthread& uthread = *reinterpret_cast<struct uthread*>(tls_addr + master_tls.size);
	uthread = {
		.self = &uthread,

		.master_tls_addr = master_tls.addr,
		.master_tls_size = master_tls.size,
		.master_tls_module_count = master_tls.module_count,
		.dynamic_tls = s_dynamic_tls,

		.id = static_cast<pid_t>(syscall(SYS_THREAD_GETID)),
		.attr = {},
		.name = "libc.so",
		.errno_ = 0,
		.libc_owns_stack = false,
		.cancel_type = PTHREAD_CANCEL_DEFERRED,
		.cancel_state = PTHREAD_CANCEL_ENABLE,
		.canceled = false,
		.cleanup_funcs = nullptr,
		.specific_keys = {},
		.specific_vals = {},

		.dtv = { master_tls.module_count }
	};

	for (auto* lookup : s_global_scope->lookup_scope)
	{
		if (!lookup->tls_header.has_value())
			continue;
		const auto& tls_header = lookup->tls_header.value();
		const ptrdiff_t offset = master_tls.size - lookup->tls_offset;
		memcpy(tls_addr + offset, master_tls.addr + offset, tls_header.p_filesz);
		memset(tls_addr + offset + tls_header.p_filesz, 0, tls_header.p_memsz - tls_header.p_filesz);
		uthread.dtv[lookup->tls_module] = reinterpret_cast<uintptr_t>(tls_addr + offset);

		ASSERT(!lookup->has_loaded_tls);
		lookup->has_loaded_tls = true;
	}

#if defined(__x86_64__)
	syscall(SYS_SET_FSBASE, &uthread);
#elif defined(__i686__)
	syscall(SYS_SET_GSBASE, &uthread);
#else
#error
#endif
}

static void rerelocate_self()
{
	auto it = s_loaded_objects.find(BAN::StringView(s_self.full_path));
	if (it == s_loaded_objects.end())
		return;
	const auto& self = *it->value;

#if defined(__x86_64__)
	constexpr uint32_t glob_dat = R_X86_64_GLOB_DAT;
#elif defined(__i686__)
	constexpr uint32_t glob_dat = R_386_GLOB_DAT;
#endif

	if (self.dynamic.rel && self.dynamic.relent)
		for (size_t i = 0; i < self.dynamic.relsz / self.dynamic.relent; i++)
			if (const auto& reloc = *reinterpret_cast<Elf_Rel*>(self.dynamic.rel + i * self.dynamic.relent); ELF_R_TYPE(reloc.r_info) == glob_dat)
				handle_relocation(self, reloc, true);
	if (self.dynamic.rela && self.dynamic.relaent)
		for (size_t i = 0; i < self.dynamic.relasz / self.dynamic.relaent; i++)
			if (const auto& reloc = *reinterpret_cast<Elf_RelA*>(self.dynamic.rela + i * self.dynamic.relaent); ELF_R_TYPE(reloc.r_info) == glob_dat)
				handle_relocation(self, reloc, true);

	if (self.dynamic.jmprel && self.dynamic.pltrelsz)
	{
		switch (self.dynamic.pltrel)
		{
			case DT_REL:
				for (size_t i = 0; i < self.dynamic.pltrelsz / sizeof(Elf_Rel); i++)
					handle_relocation(self, reinterpret_cast<Elf_Rel*>(self.dynamic.jmprel)[i], true);
				break;
			case DT_RELA:
				for (size_t i = 0; i < self.dynamic.pltrelsz / sizeof(Elf_RelA); i++)
					handle_relocation(self, reinterpret_cast<Elf_RelA*>(self.dynamic.jmprel)[i], true);
				break;
			default:
				ASSERT_NOT_REACHED();
		}
	}
}

static void copy_relocate_main_object(LoadedObject& object)
{
	if (object.dynamic.rel && object.dynamic.relent)
		for (size_t i = 0; i < object.dynamic.relsz / object.dynamic.relent; i++)
			handle_copy_relocation(object, *reinterpret_cast<Elf_Rel*>(object.dynamic.rel + i * object.dynamic.relent));
	if (object.dynamic.rela && object.dynamic.relaent)
		for (size_t i = 0; i < object.dynamic.relasz / object.dynamic.relaent; i++)
			handle_copy_relocation(object, *reinterpret_cast<Elf_RelA*>(object.dynamic.rela + i * object.dynamic.relaent));
}

static void relocate_self()
{
	static const auto crash_no_return = [][[noreturn]]() {
		asm volatile("ud2");
		__builtin_unreachable();
	};

	const auto& file_header = *reinterpret_cast<Elf_Ehdr*>(s_self.base_address);
	if (!validate_file_header(file_header))
		crash_no_return();
	s_self.entry_point = s_self.base_address + file_header.e_entry;

	// libc should not have TLS section
	for (size_t i = 0, offset = file_header.e_phoff; i < file_header.e_phnum; i++, offset += file_header.e_phentsize)
		if (const auto& program_header = *reinterpret_cast<Elf_Phdr*>(s_self.base_address + offset); program_header.p_type == PT_TLS)
			crash_no_return();

	const auto& dynamic_header = [&file_header] {
		for (size_t i = 0, offset = file_header.e_phoff; i < file_header.e_phnum; i++, offset += file_header.e_phentsize)
			if (const auto& program_header = *reinterpret_cast<Elf_Phdr*>(s_self.base_address + offset); program_header.p_type == PT_DYNAMIC)
				return program_header;
		crash_no_return();
	}();

	auto dynamics = BAN::Span { reinterpret_cast<Elf_Dyn*>(s_self.base_address + dynamic_header.p_vaddr), dynamic_header.p_memsz };
	for (size_t i = 0; i < dynamics.size(); i++)
	{
		if (dynamics[i].d_tag != DT_NULL)
			continue;
		if (dynamics[i].d_tag == DT_NEEDED)
			crash_no_return();
		dynamics = dynamics.slice(0, i);
		break;
	}

	parse_dynamic_info(s_self, dynamics);

	// relocations without symbols
	if (s_self.dynamic.rel && s_self.dynamic.relent)
		for (size_t i = 0; i < s_self.dynamic.relsz / s_self.dynamic.relent; i++)
			handle_relocation(s_self, *reinterpret_cast<Elf_Rel*>(s_self.dynamic.rel + i * s_self.dynamic.relent), false);
	if (s_self.dynamic.rela && s_self.dynamic.relaent)
		for (size_t i = 0; i < s_self.dynamic.relasz / s_self.dynamic.relaent; i++)
			handle_relocation(s_self, *reinterpret_cast<Elf_RelA*>(s_self.dynamic.rela + i * s_self.dynamic.relaent), false);

	// relocations with symbols
	if (s_self.dynamic.rel && s_self.dynamic.relent)
		for (size_t i = 0; i < s_self.dynamic.relsz / s_self.dynamic.relent; i++)
			handle_relocation(s_self, *reinterpret_cast<Elf_Rel*>(s_self.dynamic.rel + i * s_self.dynamic.relent), true);
	if (s_self.dynamic.rela && s_self.dynamic.relaent)
		for (size_t i = 0; i < s_self.dynamic.relasz / s_self.dynamic.relaent; i++)
			handle_relocation(s_self, *reinterpret_cast<Elf_RelA*>(s_self.dynamic.rela + i * s_self.dynamic.relaent), true);

	s_self.relocated = true;

	// initialize jump tables, we dont want to resolve symbols inside dynamic loader
	// TODO: find a way to only relocate functions needed by dynamic loader
	if (s_self.dynamic.jmprel && s_self.dynamic.pltrelsz)
	{
		switch (s_self.dynamic.pltrel)
		{
			case DT_REL:
				for (size_t i = 0; i < s_self.dynamic.pltrelsz / sizeof(Elf_Rel); i++)
					handle_relocation(s_self, reinterpret_cast<Elf_Rel*>(s_self.dynamic.jmprel)[i], true);
				break;
			case DT_RELA:
				for (size_t i = 0; i < s_self.dynamic.pltrelsz / sizeof(Elf_RelA); i++)
					handle_relocation(s_self, reinterpret_cast<Elf_RelA*>(s_self.dynamic.jmprel)[i], true);
				break;
			default:
				ASSERT_NOT_REACHED();
		}
	}

	// setup required GOT entries
	if (uintptr_t* pltgot = reinterpret_cast<uintptr_t*>(s_self.dynamic.pltgot))
	{
		pltgot[0] = s_self.base_address + dynamic_header.p_vaddr;
		pltgot[1] = reinterpret_cast<uintptr_t>(&s_self);
		pltgot[2] = reinterpret_cast<uintptr_t>(_resolve_symbol_trampoline);
	}

	// call init functions
	using init_t = void(*)();
	if (s_self.dynamic.init)
		reinterpret_cast<init_t>(s_self.dynamic.init)();
	for (size_t i = 0; i < s_self.dynamic.init_arraysz / sizeof(init_t); i++)
		reinterpret_cast<init_t*>(s_self.dynamic.init_array)[i]();
	s_self.called_init = true;

	// don't register lib's fini as it crashes :D
	s_self.registered_fini = true;

	s_self.file_header = file_header;
	for (size_t i = 0, offset = file_header.e_phoff; i < file_header.e_phnum; i++, offset += file_header.e_phentsize)
	{
		auto program_header = *reinterpret_cast<Elf_Phdr*>(s_self.base_address + offset);
		program_header.p_vaddr += s_self.base_address;
		MUST(s_self.program_headers.push_back(program_header));
	}
}

static Elf_auxv_t* find_auxv(char* envp[])
{
	if (envp == nullptr)
		return nullptr;

	char** null_env = envp;
	while (*null_env)
		null_env++;

	return reinterpret_cast<Elf_auxv_t*>(null_env + 1);
}

__attribute__((used))
static uintptr_t _libc_main(int argc, char* argv[], char* envp[])
{
	(void)argc; (void)argv;

	struct uthread dummy_uthread {
		.self = &dummy_uthread,

		.master_tls_addr = nullptr,
		.master_tls_size = 0,
		.master_tls_module_count = 0,
		.dynamic_tls = nullptr,

		.id = static_cast<pid_t>(_kas_syscall(SYS_GET_PID)),
		.attr = {
			.inheritsched = PTHREAD_INHERIT_SCHED,
			.schedparam   = {},
			.schedpolicy  = SCHED_RR,
			.detachstate  = PTHREAD_CREATE_JOINABLE,
			.scope        = PTHREAD_SCOPE_SYSTEM,
			.stackaddr    = nullptr,
			.stacksize    = 8 * 1024 * 1024,
			.guardsize    = 0,
		},
		.name = "libc.so",
		.errno_ = 0,
		.libc_owns_stack = false,
		.cancel_type = PTHREAD_CANCEL_DEFERRED,
		.cancel_state = PTHREAD_CANCEL_DISABLE,
		.canceled = false,
		.cleanup_funcs = nullptr,
		.specific_keys = {},
		.specific_vals = {},
		.dtv = {},
	};
#if defined(__x86_64__)
	_kas_syscall(SYS_SET_FSBASE, &dummy_uthread);
#elif defined(__i686__)
	_kas_syscall(SYS_SET_GSBASE, &dummy_uthread);
#else
#error
#endif

	BAN::Optional<uintptr_t> base;
	BAN::Optional<int> execfd;

	for (const auto* auxv = find_auxv(envp); auxv && auxv->a_type != AT_NULL; auxv++)
	{
		switch (auxv->a_type)
		{
			case AT_EXECFD:
				execfd = auxv->a_un.a_val;
				break;
			case AT_BASE:
				base = reinterpret_cast<uintptr_t>(auxv->a_un.a_ptr);
				break;
		}
	}

	ASSERT(base.has_value());
	ASSERT(execfd.has_value());

	s_self.base_address = base.value();
	relocate_self();

	ASSERT(execfd.has_value());

	char* realpath = ::realpath("/proc/self/exe", nullptr);
	if (realpath == nullptr)
	{
		fprintf(stderr, "could not get main executable path: %m\n");
		exit(1);
	}

	// FIXME: can we get this in a more portable way?
	s_self.full_path = ::realpath("/usr/lib/libc.so", nullptr);
	if (s_self.full_path == nullptr)
	{
		fprintf(stderr, "could not get libc path: %m\n");
		exit(1);
	}

	// set-uid and set-gid should not search LD_LIBRARY_PATH
	if (struct stat st; stat(realpath, &st) == 0 && !(st.st_mode & (S_ISUID | S_ISGID)))
	{
		for (size_t i = 0; envp[i]; i++)
		{
			if (strncmp(envp[i], "LD_LIBRARY_PATH=", 16) == 0)
				s_ld_library_path = envp[i] + 16;
			if (strncmp(envp[i], "LD_PRELOAD=", 11) == 0)
				s_ld_preload = envp[i] + 11;
		}
	}

	auto* object = load_object(realpath, execfd.value(), false);
	if (object == nullptr)
	{
		fprintf(stderr, "could not load main executable: %m\n");
		exit(1);
	}

	s_global_scope = object;
	build_lookup_scope(*object);
	add_scope_root(*object, s_global_scope);

	const auto master_tls = initialize_tls_stage1();

	bool bind_now = false;
	for (size_t i = 0; envp[i]; i++)
		if (strncmp(envp[i], "LD_BIND_NOW=", 12) == 0 && envp[i][12])
			bind_now = true;
	relocate_object(*object, bind_now);

	copy_relocate_main_object(*object);

	rerelocate_self();

	initialize_tls_stage2(master_tls);

	if (const auto sym = find_symbol_in_scope(*object, "environ"_sv); sym.address)
		*reinterpret_cast<char***>(sym.address) = envp;

	call_init_funcs(*object, true);
	register_fini_funcs(*object, true);

	close(execfd.value());

	return object->entry_point;
}

__attribute__((naked))
void _libc_entry()
{
#if defined(__x86_64__)
	asm volatile(
		"movq  (%rsp), %rdi;"
		"leaq 8(%rsp), %rsi;"
		"leaq 8(%rsi, %rdi, 8), %rdx;"

		"movq %rsp, %rbp;"
		"andq $-16, %rsp;"

		"call _libc_main;"

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

		"call _libc_main;"

		"movl %ebp, %esp;"
		"xorl %ebp, %ebp;"

		"jmp *%eax;"

		"ud2;"
	);
#else
	#error "unsupported architecture"
#endif
}

__attribute__((naked))
static void _resolve_symbol_trampoline()
{
#if defined(__x86_64__)
	asm volatile(
		"subq $8, %rsp;"
		"pushq %rdi;"
		"pushq %rsi;"
		"pushq %rdx;"
		"pushq %rcx;"
		"pushq %r8;"
		"pushq %r9;"
		"pushq %r10;"
		"pushq %r11;"

		"subq $0x80, %rsp;"
		"movdqa %xmm7, 0x70(%rsp);"
		"movdqa %xmm6, 0x60(%rsp);"
		"movdqa %xmm5, 0x50(%rsp);"
		"movdqa %xmm4, 0x40(%rsp);"
		"movdqa %xmm3, 0x30(%rsp);"
		"movdqa %xmm2, 0x20(%rsp);"
		"movdqa %xmm1, 0x10(%rsp);"
		"movdqa %xmm0, 0x00(%rsp);"

		"movq 72+0x80(%rsp), %rdi;"
		"movq 80+0x80(%rsp), %rsi;"
		"call _resolve_symbol;"

		"movdqa 0x70(%rsp), %xmm7;"
		"movdqa 0x60(%rsp), %xmm6;"
		"movdqa 0x50(%rsp), %xmm5;"
		"movdqa 0x40(%rsp), %xmm4;"
		"movdqa 0x30(%rsp), %xmm3;"
		"movdqa 0x20(%rsp), %xmm2;"
		"movdqa 0x10(%rsp), %xmm1;"
		"movdqa 0x00(%rsp), %xmm0;"
		"addq $0x80, %rsp;"

		"popq %r11;"
		"popq %r10;"
		"popq %r9;"
		"popq %r8;"
		"popq %rcx;"
		"popq %rdx;"
		"popq %rsi;"
		"popq %rdi;"

		"addq $(8+16), %rsp;"
		"jmp *%rax;"
	);
#elif defined(__i686__)
	asm volatile(
		"call _resolve_symbol;"
		"addl $8, %esp;"
		"jmp *%eax;"
	);
#else
	#error "unsupported architecture"
#endif
}
