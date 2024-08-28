#include <BAN/ScopeGuard.h>
#include <kernel/FS/VirtualFileSystem.h>
#include <kernel/Lock/LockGuard.h>
#include <kernel/Memory/Heap.h>
#include <kernel/Random.h>
#include <LibELF/LoadableELF.h>
#include <LibELF/Values.h>
#include <fcntl.h>

namespace LibELF
{

	using namespace Kernel;

	BAN::ErrorOr<BAN::UniqPtr<LoadableELF>> LoadableELF::load_from_inode(PageTable& page_table, const Credentials& credentials, BAN::RefPtr<Inode> inode)
	{
		auto elf = TRY(BAN::UniqPtr<LoadableELF>::create(page_table));
		TRY(elf->initialize(credentials, inode));
		return elf;
	}

	LoadableELF::LoadableELF(PageTable& page_table)
		: m_page_table(page_table)
	{
	}

	LoadableELF::~LoadableELF()
	{
		if (!m_is_loaded)
			return;

		for (const auto& header : m_program_headers)
		{
			ASSERT(header.p_type == PT_LOAD);

			const vaddr_t vaddr = header.p_vaddr & PAGE_ADDR_MASK;
			const size_t pages = range_page_count(header.p_vaddr, header.p_memsz);
			for (size_t i = 0; i < pages; i++)
				if (paddr_t paddr = m_page_table.physical_address_of(vaddr + i * PAGE_SIZE))
					Heap::get().release_page(paddr);
			m_page_table.unmap_range(vaddr, pages * PAGE_SIZE);
		}
	}

	static BAN::ErrorOr<ElfNativeFileHeader> read_and_validate_file_header(BAN::RefPtr<Inode> inode)
	{
		if ((size_t)inode->size() < sizeof(ElfNativeFileHeader))
		{
			dprintln("File is too small to be ELF");
			return BAN::Error::from_errno(ENOEXEC);
		}

		ElfNativeFileHeader file_header;

		size_t nread = TRY(inode->read(0, BAN::ByteSpan::from(file_header)));
		ASSERT(nread == sizeof(file_header));

		if (file_header.e_ident[EI_MAG0] != ELFMAG0 ||
			file_header.e_ident[EI_MAG1] != ELFMAG1 ||
			file_header.e_ident[EI_MAG2] != ELFMAG2 ||
			file_header.e_ident[EI_MAG3] != ELFMAG3)
		{
			dprintln("Not an ELF file");
			return BAN::Error::from_errno(ENOEXEC);
		}

		if (file_header.e_ident[EI_DATA] != ELFDATA2LSB)
		{
			dprintln("Not in little-endian");
			return BAN::Error::from_errno(ENOEXEC);
		}

		if (file_header.e_ident[EI_VERSION] != EV_CURRENT)
		{
			dprintln("Unsupported version {}", file_header.e_ident[EI_VERSION]);
			return BAN::Error::from_errno(ENOEXEC);
		}

#if ARCH(i686)
		if (file_header.e_ident[EI_CLASS] != ELFCLASS32)
#elif ARCH(x86_64)
		if (file_header.e_ident[EI_CLASS] != ELFCLASS64)
#endif
		{
			dprintln("Not in native format");
			return BAN::Error::from_errno(EINVAL);
		}

		if (file_header.e_type != ET_EXEC && file_header.e_type != ET_DYN)
		{
			dprintln("Unsupported file header type {}", file_header.e_type);
			return BAN::Error::from_errno(ENOTSUP);
		}

		if (file_header.e_version != EV_CURRENT)
		{
			dprintln("Unsupported version {}", file_header.e_version);
			return BAN::Error::from_errno(EINVAL);
		}

		if (file_header.e_phentsize < sizeof(ElfNativeProgramHeader))
		{
			dprintln("Too small program header size ({} bytes)", file_header.e_phentsize);
			return BAN::Error::from_errno(EINVAL);
		}

		return file_header;
	}

	BAN::ErrorOr<LoadableELF::LoadResult> LoadableELF::load_elf_file(const Credentials& credentials, BAN::RefPtr<Inode> inode) const
	{
		auto file_header = TRY(read_and_validate_file_header(inode));

		BAN::Vector<uint8_t> pheader_buffer;
		TRY(pheader_buffer.resize(file_header.e_phnum * file_header.e_phentsize));
		TRY(inode->read(file_header.e_phoff, BAN::ByteSpan(pheader_buffer.span())));

		BAN::Vector<ElfNativeProgramHeader> program_headers;
		BAN::RefPtr<Inode> interp;

		for (size_t i = 0; i < file_header.e_phnum; i++)
		{
			const auto& pheader = *reinterpret_cast<ElfNativeProgramHeader*>(pheader_buffer.data() + i * file_header.e_phentsize);
			if (pheader.p_memsz < pheader.p_filesz)
			{
				dprintln("Invalid program header, memsz less than filesz");
				return BAN::Error::from_errno(EINVAL);
			}

			switch (pheader.p_type)
			{
				case PT_LOAD:
					for (const auto& program_header : program_headers)
					{
						const vaddr_t a1 = program_header.p_vaddr & PAGE_ADDR_MASK;
						const vaddr_t b1 = pheader.p_vaddr        & PAGE_ADDR_MASK;
						const vaddr_t a2 = (program_header.p_vaddr + program_header.p_memsz + PAGE_SIZE - 1) & PAGE_ADDR_MASK;
						const vaddr_t b2 = (pheader.p_vaddr        + pheader.p_memsz        + PAGE_SIZE - 1) & PAGE_ADDR_MASK;
						if (a1 < b2 && b1 < a2)
						{
							dwarnln("Overlapping LOAD segments");
							return BAN::Error::from_errno(EINVAL);
						}
					}
					TRY(program_headers.push_back(pheader));
					break;
				case PT_INTERP:
				{
					BAN::Vector<uint8_t> buffer;
					TRY(buffer.resize(pheader.p_memsz, 0));
					TRY(inode->read(pheader.p_offset, BAN::ByteSpan(buffer.data(), pheader.p_filesz)));

					BAN::StringView path(reinterpret_cast<const char*>(buffer.data()));
					interp = TRY(VirtualFileSystem::get().file_from_absolute_path(credentials, path, O_EXEC)).inode;
					break;
				}
				default:
					break;
			}
		}

		return LoadResult {
			.inode = inode,
			.interp = interp,
			.file_header = file_header,
			.program_headers = BAN::move(program_headers)
		};
	}

	static bool do_program_headers_overlap(BAN::Span<const ElfNativeProgramHeader> pheaders1, BAN::Span<const ElfNativeProgramHeader> pheaders2, vaddr_t base2)
	{
		for (const auto& pheader1 : pheaders1)
		{
			for (const auto& pheader2 : pheaders2)
			{
				const vaddr_t s1 =  pheader1.p_vaddr & PAGE_ADDR_MASK;
				const vaddr_t e1 = (pheader1.p_vaddr + pheader1.p_memsz + PAGE_SIZE - 1) & PAGE_ADDR_MASK;

				const vaddr_t s2 =  pheader2.p_vaddr & PAGE_ADDR_MASK;
				const vaddr_t e2 = (pheader2.p_vaddr + pheader2.p_memsz + PAGE_SIZE - 1) & PAGE_ADDR_MASK;

				if (s1 < e2 + base2 && s2 + base2 < e1)
					return true;
			}
		}

		return false;
	}

	BAN::ErrorOr<void> LoadableELF::initialize(const Credentials& credentials, BAN::RefPtr<Inode> inode)
	{
		const auto generate_random_dynamic_base =
			[]() -> vaddr_t
			{
				// 1 MiB -> 2 GiB + 1 MiB
				return (Random::get_u32() & 0x7FFFF000) + 0x100000;
			};


		auto executable_load_result = TRY(load_elf_file(credentials, inode));

		m_executable = executable_load_result.inode;
		m_interpreter = executable_load_result.interp;

		vaddr_t dynamic_base = 0;

		if (m_interpreter)
		{
			auto interp_load_result = TRY(load_elf_file(credentials, m_interpreter));
			if (interp_load_result.interp)
			{
				dwarnln("ELF interpreter has an interpreter");
				return BAN::Error::from_errno(EINVAL);
			}

			if (executable_load_result.file_header.e_type == ET_EXEC)
			{
				if (interp_load_result.file_header.e_type == ET_EXEC)
				{
					const bool has_overlap = do_program_headers_overlap(
						executable_load_result.program_headers.span(),
						interp_load_result.program_headers.span(),
						0
					);

					if (has_overlap)
					{
						dwarnln("Executable and interpreter LOAD segments overlap");
						return BAN::Error::from_errno(EINVAL);
					}
				}
				else
				{
					for (int attempt = 0; attempt < 100; attempt++)
					{
						const vaddr_t test_dynamic_base = generate_random_dynamic_base();
						const bool has_overlap = do_program_headers_overlap(
							executable_load_result.program_headers.span(),
							interp_load_result.program_headers.span(),
							test_dynamic_base
						);
						if (has_overlap)
							continue;
						dynamic_base = test_dynamic_base;
						break;
					}

					if (dynamic_base == 0)
					{
						dwarnln("Could not find space to load interpreter");
						return BAN::Error::from_errno(EINVAL);
					}
				}
			}

			m_file_header = interp_load_result.file_header;
			m_program_headers = BAN::move(interp_load_result.program_headers);
		}
		else
		{
			m_file_header = executable_load_result.file_header;
			m_program_headers = BAN::move(executable_load_result.program_headers);
		}

		if (m_file_header.e_type == ET_DYN && dynamic_base == 0)
			dynamic_base = generate_random_dynamic_base();

		if (dynamic_base)
		{
			m_file_header.e_entry += dynamic_base;
			for (auto& program_header : m_program_headers)
				program_header.p_vaddr += dynamic_base;
		}

		return {};
	}

	bool LoadableELF::contains(vaddr_t address) const
	{
		for (const auto& program_header : m_program_headers)
			if (program_header.p_vaddr <= address && address < program_header.p_vaddr + program_header.p_memsz)
				return true;
		return false;
	}

	bool LoadableELF::is_address_space_free() const
	{
		for (const auto& program_header : m_program_headers)
		{
			ASSERT(program_header.p_type == PT_LOAD);
			const vaddr_t page_vaddr = program_header.p_vaddr & PAGE_ADDR_MASK;
			const size_t pages = range_page_count(program_header.p_vaddr, program_header.p_memsz);
			if (!m_page_table.is_range_free(page_vaddr, pages * PAGE_SIZE))
				return false;
		}
		return true;
	}

	void LoadableELF::reserve_address_space()
	{
		for (const auto& program_header : m_program_headers)
		{
			ASSERT(program_header.p_type == PT_LOAD);
			const vaddr_t page_vaddr = program_header.p_vaddr & PAGE_ADDR_MASK;
			const size_t pages = range_page_count(program_header.p_vaddr, program_header.p_memsz);
			if (!m_page_table.reserve_range(page_vaddr, pages * PAGE_SIZE))
				ASSERT_NOT_REACHED();
			m_virtual_page_count += pages;
		}
		m_is_loaded = true;
	}

	void LoadableELF::update_suid_sgid(Kernel::Credentials& credentials)
	{
		if (m_executable->mode().mode & +Inode::Mode::ISUID)
			credentials.set_euid(m_executable->uid());
		if (m_executable->mode().mode & +Inode::Mode::ISGID)
			credentials.set_egid(m_executable->gid());
	}

	BAN::ErrorOr<void> LoadableELF::load_page_to_memory(vaddr_t address)
	{
		auto inode = has_interpreter() ? m_interpreter : m_executable;

		// FIXME: use MemoryBackedRegion/FileBackedRegion instead of manually mapping and allocating pages

		for (const auto& program_header : m_program_headers)
		{
			ASSERT(program_header.p_type == PT_LOAD);
			if (!(program_header.p_vaddr <= address && address < program_header.p_vaddr + program_header.p_memsz))
				continue;

			PageTable::flags_t flags = PageTable::Flags::UserSupervisor | PageTable::Flags::Present;
			if (program_header.p_flags & LibELF::PF_W)
				flags |= PageTable::Flags::ReadWrite;
			if (program_header.p_flags & LibELF::PF_X)
				flags |= PageTable::Flags::Execute;

			const vaddr_t vaddr = address & PAGE_ADDR_MASK;
			const paddr_t paddr = Heap::get().take_free_page();
			if (paddr == 0)
				return BAN::Error::from_errno(ENOMEM);

			// Temporarily map page as RW so kernel can write to it
			m_page_table.map_page_at(paddr, vaddr, PageTable::Flags::ReadWrite | PageTable::Flags::Present);
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
				TRY(inode->read(program_header.p_offset + file_offset, { (uint8_t*)vaddr + vaddr_offset, bytes }));
			}

			// Map page with the correct flags
			m_page_table.map_page_at(paddr, vaddr, flags);

			return {};
		}

		ASSERT_NOT_REACHED();
	}

	BAN::ErrorOr<BAN::UniqPtr<LoadableELF>> LoadableELF::clone(Kernel::PageTable& new_page_table)
	{
		auto elf = TRY(BAN::UniqPtr<LoadableELF>::create(new_page_table));

		elf->m_executable = m_executable;
		elf->m_interpreter = m_interpreter;
		elf->m_file_header = m_file_header;
		TRY(elf->m_program_headers.reserve(m_program_headers.size()));
		for (const auto& program_header : m_program_headers)
			MUST(elf->m_program_headers.emplace_back(program_header));

		elf->reserve_address_space();

		for (const auto& program_header : m_program_headers)
		{
			ASSERT(program_header.p_type == PT_LOAD);
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

				PageTable::with_fast_page(paddr, [&] {
					memcpy(PageTable::fast_page_as_ptr(), (void*)(start + i * PAGE_SIZE), PAGE_SIZE);
				});

				new_page_table.map_page_at(paddr, start + i * PAGE_SIZE, flags);
				elf->m_physical_page_count++;
			}
		}

		return elf;
	}

}
