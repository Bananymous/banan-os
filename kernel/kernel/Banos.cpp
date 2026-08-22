#include <kernel/Debug.h>
#include <kernel/Banos.h>
#include <BAN/Assert.h>
#include <banos/driver.h>
#include <banos/print.h>
#include <banos/export.h>
#include <kernel/FS/VirtualFileSystem.h>
#include <kernel/Memory/PageTable.h>
#include <kernel/ELF.h>
#include <LibELF/Types.h>
#include <LibELF/Values.h>
#include <kernel/Process.h>
#include <BAN/HashMap.h>
#include <kernel/Lock/SpinLock.h>
#include <kernel/UserCopy.h>

#if ARCH(x86_64)

using namespace LibELF;
using namespace Kernel;

extern "C" {
	void banos_dprintln(const char* str) {
		dprintln("{}", str);
	}
	void* banos_lookup_symbol(const char* str) {
		return Banos::resolve_symbol(str);
	}
}
BANOS_EXPORT(banos_dprintln);
BANOS_EXPORT(banos_lookup_symbol);

BAN::HashMap<BAN::StringView, void*> g_banos_symbols;
void* Banos::resolve_symbol(const char* name) {
	auto it = g_banos_symbols.find(name);
	return it == g_banos_symbols.end() ? NULL : it->value;
}
void Banos::import_symbols(Banos_Symbol* symbols, size_t count) {
	for(size_t i = 0; i < count; ++i) {
		auto sym = symbols + i;
		MUST(g_banos_symbols.insert(sym->name, sym->arg));
	}
}
// TODO: driver unloading with a reference counter
struct Driver_Instance {
	Banos_Driver* drv;
};
static BAN::Vector<Driver_Instance> s_driver_instaces;
static SpinLock s_driver_instaces_lock;

extern Banos_Symbol g_banos_export[],
					g_banos_export_end[];
static void load_drv(Banos_Driver* drv) {
	ASSERT(drv->driver_size >= sizeof(Banos_Driver));
	dprintln("Loading driver:");
	dprintln("  name: {}", drv->name);
	if(drv->license) dprintln("  license: {}", drv->license);
	dprintln("  version: {}.{}.{}", BANOS_VERSION_GET_MAJOR(drv->version), BANOS_VERSION_GET_MINOR(drv->version), BANOS_VERSION_GET_PATCH(drv->version));
	int e = drv->init(drv);
	if(e < 0) dprintln(" Failed to init {} => {}", drv->name, -e);
}
BAN::ErrorOr<size_t> Banos::load_driver_from_image(const char* u_image) {
	if(!Process::current().credentials().is_superuser()) return BAN::Error::from_errno(EPERM);
	// TODO: permission verification. Only root should be allowed to do this
	LibELF::ElfNativeFileHeader header;

	const unsigned char elf_class =
		#if ARCH(i686)
			ELFCLASS32;
		#elif ARCH(x86_64)
			ELFCLASS64;
		#else
		#   error update elf class
		#endif

	// TODO: is banan-os really ever gonna be running on MSB machines?
	const unsigned char elf_data = ELFDATA2LSB;
	// TODO: do we need to verify e_machine? I mean we do not really care.
	// But I'm leaving this todo:
	//   Look up EM_X86_64 and EM_360|EM_860|EM_960
	TRY(read_from_user(u_image, &header, sizeof header));
	if( header.e_ident[EI_MAG0]	   != ELFMAG0        ||
		header.e_ident[EI_MAG1]	   != ELFMAG1        ||
		header.e_ident[EI_MAG2]	   != ELFMAG2        ||
		header.e_ident[EI_MAG3]	   != ELFMAG3        ||
		header.e_ident[EI_CLASS]   != elf_class	     ||
		header.e_ident[EI_DATA]	   != elf_data       ||
		header.e_ident[EI_VERSION] != EV_CURRENT     ||
		header.e_type			   != ET_REL         ||
		header.e_version		   != EV_CURRENT     ||
		header.e_ehsize			   != sizeof(header) ||
		header.e_shentsize		   != sizeof(ElfNativeSectionHeader))
		return BAN::Error::from_errno(EINVAL);


	BAN::Vector<LibELF::ElfNativeSectionHeader> secs(header.e_shnum);
	TRY(read_from_user(u_image + header.e_shoff, secs.data(), secs.size() * sizeof(*secs.data())));
	auto shstr = secs[header.e_shstrndx];

	size_t total_size = 0;

	LibELF::ElfNativeSectionHeader  *strtab = nullptr,
									*symtab = nullptr,
									*driver_section = nullptr;

	for(auto& sec : secs) {
		if(sec.sh_flags & LibELF::SHF_ALLOC) {
			sec.sh_addr = total_size;
			total_size += sec.sh_size;
		}
		if(sec.sh_name == 0) continue;
		char name[256];
		TRY(read_string_from_user(u_image + shstr.sh_offset + sec.sh_name, name, sizeof name));
		BAN::StringView name_sv(name);

		if(sec.sh_type == LibELF::SHT_SYMTAB) {
			symtab = &sec;
		}
		// TODO: verify sh_type for both of these?
		if(name_sv == ".strtab") {
			strtab = &sec;
		} else if(name_sv == ".banos-driver") {
			driver_section = &sec;
		}
	}
	if(!symtab || !strtab || !driver_section)
		return BAN::Error::from_errno(EINVAL);
	total_size += PAGE_SIZE;
	total_size &= ~(PAGE_SIZE-1);
	auto driver = TRY(VirtualRange::create_to_vaddr_range(PageTable::kernel(), { KERNEL_OFFSET, UINTPTR_MAX }, total_size, PageTable::Execute | PageTable::ReadWrite | PageTable::Present, true));

	for(auto& sec : secs) {
		if(sec.sh_flags & LibELF::SHF_ALLOC) {
			sec.sh_addr += driver->vaddr();
		}
	}
	Banos_Driver* banos_driver = reinterpret_cast<Banos_Driver*>(driver_section->sh_addr);
	for(auto& sec : secs) {
		if(sec.sh_name == 0) continue;
		if(sec.sh_flags & LibELF::SHF_ALLOC) {
			TRY(read_from_user(u_image + sec.sh_offset, reinterpret_cast<char*>(sec.sh_addr), sec.sh_size));
		}
		if(sec.sh_type == LibELF::SHT_RELA) {
			auto& link_sec = secs[sec.sh_info];
			size_t rela_count = sec.sh_size/sizeof(LibELF::ElfNativeRelocationA);
			BAN::Vector<LibELF::ElfNativeRelocationA> rela_data(rela_count);
			TRY(read_from_user(u_image + sec.sh_offset, rela_data.data(), rela_count * sizeof *rela_data.data()));

			for(auto rela : rela_data) {
				auto type   = ELF64_R_TYPE(rela.r_info);
				auto symbol = ELF64_R_SYM(rela.r_info);

				vaddr_t value = 0;

				LibELF::ElfNativeSymbol sym;
				TRY(read_from_user(u_image + symtab->sh_offset + sizeof(sym) * symbol, &sym, sizeof sym));

				if(sym.st_shndx) {
					value = secs[sym.st_shndx].sh_addr;
				} else {
					char name[256];
					TRY(read_string_from_user(u_image + strtab->sh_offset + sym.st_name, name, sizeof name));

					value = reinterpret_cast<vaddr_t>(Banos::resolve_symbol(name));
					if(!value) {
						derrorln("Failed to find symbol {}", name);
						return BAN::Error::from_errno(ENOENT);
					}
				}
				vaddr_t at = link_sec.sh_addr + rela.r_offset;
				size_t size = 0;
				switch(type) {
				case LibELF::R_X86_64_PLT32:
				case LibELF::R_X86_64_PC32:
					value -= at;
					// fallthrough
				case LibELF::R_X86_64_32:
				case LibELF::R_X86_64_32S:
					value += rela.r_addend;
					size = sizeof(uint32_t);
					break;
				case LibELF::R_X86_64_64:
					value += rela.r_addend;
					size = sizeof(uint64_t);
					break;
				default:
					derrorln("TODO: Unsupported relocation type {}", type);
					return BAN::Error::from_errno(ENOSYS);
				}

				switch(size) {
				case 4: *reinterpret_cast<uint32_t*>(at) = value; break;
				case 8: *reinterpret_cast<uint64_t*>(at) = value; break;
				}
			}
		}
	}
	Driver_Instance instance;
	instance.drv = banos_driver;
	load_drv(instance.drv);
	SpinLockGuard _(s_driver_instaces_lock);
	TRY(s_driver_instaces.push_back(instance));
	// TODO: import symbols and resolve redefintions :)
	return s_driver_instaces.size() - 1;
}
// NOTE: should be more than plenty ;)
extern char g_drv_builtin_begin[];
extern char g_drv_builtin_end[];
void Banos::initialize_initial_drivers() {
	import_symbols(g_banos_export, g_banos_export_end - g_banos_export);
	char* head = g_drv_builtin_begin;
	while(head < g_drv_builtin_end) {
		Banos_Driver* drv = (Banos_Driver*)head;
		load_drv(drv);
		head += drv->driver_size;
	}
}

#else
void Banos::initialize_initial_drivers()
{
}
BAN::ErrorOr<size_t> Banos::load_driver_from_image(const char* u_image)
{
	(void)u_image;
	return BAN::Error::from_errno(ENOTSUP);
}
#endif
