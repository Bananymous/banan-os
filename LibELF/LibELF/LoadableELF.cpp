#include <BAN/ScopeGuard.h>
#include <kernel/Memory/Heap.h>
#include <kernel/LockGuard.h>
#include <LibELF/LoadableELF.h>
#include <LibELF/Values.h>

namespace LibELF
{

	using namespace Kernel;

	BAN::ErrorOr<BAN::UniqPtr<LoadableELF>> LoadableELF::load_from_inode(PageTable& page_table, BAN::RefPtr<Inode> inode)
	{
		auto* elf_ptr = new LoadableELF(page_table, inode);
		if (elf_ptr == nullptr)
			return BAN::Error::from_errno(ENOMEM);
		auto elf = BAN::UniqPtr<LoadableELF>::adopt(elf_ptr);
		TRY(elf->initialize());
		return BAN::move(elf);
	}

	LoadableELF::LoadableELF(PageTable& page_table, BAN::RefPtr<Inode> inode)
		: m_inode(inode)
		, m_page_table(page_table)
	{
	}

	LoadableELF::~LoadableELF()
	{
		if (!m_loaded)
			return;
		for (const auto& program_header : m_program_headers)
		{
			switch (program_header.p_type)
			{
				case PT_NULL:
					continue;
				case PT_LOAD:
				{
					vaddr_t start = program_header.p_vaddr & PAGE_ADDR_MASK;
					size_t pages = range_page_count(program_header.p_vaddr, program_header.p_memsz);
					for (size_t i = 0; i < pages; i++)
					{
						paddr_t paddr = m_page_table.physical_address_of(start + i * PAGE_SIZE);
						if (paddr != 0)
							Heap::get().release_page(paddr);
					}
					m_page_table.unmap_range(start, pages * PAGE_SIZE);
					break;
				}
				default:
					ASSERT_NOT_REACHED();
			}
		}
	}

	BAN::ErrorOr<void> LoadableELF::initialize()
	{
		if ((size_t)m_inode->size() < sizeof(ElfNativeFileHeader))
		{
			dprintln("Too small file");
			return BAN::Error::from_errno(ENOEXEC);
		}

		size_t nread = TRY(m_inode->read(0, &m_file_header, sizeof(m_file_header)));
		ASSERT(nread == sizeof(m_file_header));

		if (m_file_header.e_ident[EI_MAG0] != ELFMAG0 || 
			m_file_header.e_ident[EI_MAG1] != ELFMAG1 ||
			m_file_header.e_ident[EI_MAG2] != ELFMAG2 ||
			m_file_header.e_ident[EI_MAG3] != ELFMAG3)
		{
			dprintln("Invalid magic in header");
			return BAN::Error::from_errno(ENOEXEC);
		}

		if (m_file_header.e_ident[EI_DATA] != ELFDATA2LSB)
		{
			dprintln("Only little-endian is supported");
			return BAN::Error::from_errno(ENOEXEC);
		}

		if (m_file_header.e_ident[EI_VERSION] != EV_CURRENT)
		{
			dprintln("Invalid version");
			return BAN::Error::from_errno(ENOEXEC);
		}

#if ARCH(i386)
		if (m_file_header.e_ident[EI_CLASS] != ELFCLASS32)
#elif ARCH(x86_64)
		if (m_file_header.e_ident[EI_CLASS] != ELFCLASS64)
#endif
		{
			dprintln("Not in native format");	
			return BAN::Error::from_errno(EINVAL);
		}

		if (m_file_header.e_type != ET_EXEC)
		{
			dprintln("Only executable files are supported");
			return BAN::Error::from_errno(EINVAL);
		}

		if (m_file_header.e_version != EV_CURRENT)
		{
			dprintln("Unsupported version");
			return BAN::Error::from_errno(EINVAL);
		}

		ASSERT(m_file_header.e_phentsize <= sizeof(ElfNativeProgramHeader));

		TRY(m_program_headers.resize(m_file_header.e_phnum));
		for (size_t i = 0; i < m_file_header.e_phnum; i++)
		{
			TRY(m_inode->read(m_file_header.e_phoff + m_file_header.e_phentsize * i, &m_program_headers[i], m_file_header.e_phentsize));

			const auto& pheader = m_program_headers[i];
			if (pheader.p_type != PT_NULL && pheader.p_type != PT_LOAD)
			{
				dprintln("Unsupported program header type {}", pheader.p_type);
				return BAN::Error::from_errno(ENOTSUP);
			}
			if (pheader.p_memsz < pheader.p_filesz)
			{
				dprintln("Invalid program header");
				return BAN::Error::from_errno(EINVAL);
			}

			m_virtual_page_count += BAN::Math::div_round_up<size_t>((pheader.p_vaddr % PAGE_SIZE) + pheader.p_memsz, PAGE_SIZE);
		}

		return {};
	}

	vaddr_t LoadableELF::entry_point() const
	{
		return m_file_header.e_entry;
	}

	bool LoadableELF::contains(vaddr_t address) const
	{
		for (const auto& program_header : m_program_headers)
		{
			switch (program_header.p_type)
			{
				case PT_NULL:
					continue;
				case PT_LOAD:
					if (program_header.p_vaddr <= address && address < program_header.p_vaddr + program_header.p_memsz)
						return true;
					break;
				default:
					ASSERT_NOT_REACHED();
			}
		}
		return false;
	}

	bool LoadableELF::is_address_space_free() const
	{
		for (const auto& program_header : m_program_headers)
		{
			switch (program_header.p_type)
			{
				case PT_NULL:
					break;
				case PT_LOAD:
				{
					vaddr_t page_vaddr = program_header.p_vaddr & PAGE_ADDR_MASK;
					size_t pages = range_page_count(program_header.p_vaddr, program_header.p_memsz);
					if (!m_page_table.is_range_free(page_vaddr, pages * PAGE_SIZE))
						return false;
					break;
				}
				default:
					ASSERT_NOT_REACHED();
			}
		}
		return true;
	}

	void LoadableELF::reserve_address_space()
	{
		for (const auto& program_header : m_program_headers)
		{
			switch (program_header.p_type)
			{
				case PT_NULL:
					break;
				case PT_LOAD:
				{
					vaddr_t page_vaddr = program_header.p_vaddr & PAGE_ADDR_MASK;
					size_t pages = range_page_count(program_header.p_vaddr, program_header.p_memsz);
					ASSERT(m_page_table.reserve_range(page_vaddr, pages * PAGE_SIZE));
					break;
				}
				default:
					ASSERT_NOT_REACHED();
			}
		}
		m_loaded = true;
	}

	BAN::ErrorOr<void> LoadableELF::load_page_to_memory(vaddr_t address)
	{
		for (const auto& program_header : m_program_headers)
		{
			switch (program_header.p_type)
			{
				case PT_NULL:
					break;
				case PT_LOAD:
				{
					if (!(program_header.p_vaddr <= address && address < program_header.p_vaddr + program_header.p_memsz))
						continue;

					PageTable::flags_t flags = PageTable::Flags::UserSupervisor | PageTable::Flags::Present;
					if (program_header.p_flags & LibELF::PF_W)
						flags |= PageTable::Flags::ReadWrite;
					if (program_header.p_flags & LibELF::PF_X)
						flags |= PageTable::Flags::Execute;

					vaddr_t vaddr = address & PAGE_ADDR_MASK;
					paddr_t paddr = Heap::get().take_free_page();
					if (paddr == 0)
						return BAN::Error::from_errno(ENOMEM);

					m_page_table.map_page_at(paddr, vaddr, flags);
					m_physical_page_count++;

					memset((void*)vaddr, 0x00, PAGE_SIZE);
					
					if (vaddr / PAGE_SIZE < BAN::Math::div_round_up<size_t>(program_header.p_vaddr + program_header.p_filesz, PAGE_SIZE))
					{
						size_t vaddr_offset = 0;
						if (vaddr < program_header.p_vaddr)
							vaddr_offset = program_header.p_vaddr - vaddr;
						
						size_t file_offset = 0;
						if (vaddr > program_header.p_vaddr)
							file_offset = vaddr - program_header.p_vaddr;

						size_t bytes = BAN::Math::min<size_t>(PAGE_SIZE - vaddr_offset, program_header.p_filesz - file_offset);
						TRY(m_inode->read(program_header.p_offset + file_offset, (void*)(vaddr + vaddr_offset), bytes));
					}

					return {};
				}
				default:
					ASSERT_NOT_REACHED();
			}
		}
		ASSERT_NOT_REACHED();
	}

	
	BAN::ErrorOr<BAN::UniqPtr<LoadableELF>> LoadableELF::clone(Kernel::PageTable& new_page_table)
	{
		auto* elf_ptr = new LoadableELF(new_page_table, m_inode);
		if (elf_ptr == nullptr)
			return BAN::Error::from_errno(ENOMEM);
		auto elf = BAN::UniqPtr<LoadableELF>::adopt(elf_ptr);

		memcpy(&elf->m_file_header, &m_file_header, sizeof(ElfNativeFileHeader));

		TRY(elf->m_program_headers.resize(m_program_headers.size()));
		memcpy(elf->m_program_headers.data(), m_program_headers.data(), m_program_headers.size() * sizeof(ElfNativeProgramHeader));

		elf->reserve_address_space();

		ASSERT(&PageTable::current() == &m_page_table);
		LockGuard _(m_page_table);
		ASSERT(m_page_table.is_page_free(0));

		for (const auto& program_header : m_program_headers)
		{
			switch (program_header.p_type)
			{
				case PT_NULL:
					break;
				case PT_LOAD:
				{
					if (!(program_header.p_flags & LibELF::PF_W))
						continue;

					PageTable::flags_t flags = PageTable::Flags::UserSupervisor | PageTable::Flags::Present;
					if (program_header.p_flags & LibELF::PF_W)
						flags |= PageTable::Flags::ReadWrite;
					if (program_header.p_flags & LibELF::PF_X)
						flags |= PageTable::Flags::Execute;

					vaddr_t start = program_header.p_vaddr & PAGE_ADDR_MASK;
					size_t pages = range_page_count(program_header.p_vaddr, program_header.p_memsz);

					for (size_t i = 0; i < pages; i++)
					{
						if (m_page_table.physical_address_of(start + i * PAGE_SIZE) == 0)
							continue;

						paddr_t paddr = Heap::get().take_free_page();
						if (paddr == 0)
							return BAN::Error::from_errno(ENOMEM);

						m_page_table.map_page_at(paddr, 0, PageTable::Flags::ReadWrite | PageTable::Flags::Present);
						memcpy((void*)0, (void*)(start + i * PAGE_SIZE), PAGE_SIZE);
						m_page_table.unmap_page(0);

						new_page_table.map_page_at(paddr, start + i * PAGE_SIZE, flags);
						elf->m_physical_page_count++;
					}

					break;
				}
				default:
					ASSERT_NOT_REACHED();
			}
		}

		return elf;
	}

}
