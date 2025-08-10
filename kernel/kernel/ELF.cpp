#include <kernel/ELF.h>
#include <kernel/FS/VirtualFileSystem.h>
#include <kernel/Memory/FileBackedRegion.h>
#include <kernel/Memory/MemoryBackedRegion.h>

#include <LibELF/Types.h>
#include <LibELF/Values.h>

#include <ctype.h>
#include <fcntl.h>
#include <pthread.h>

namespace Kernel::ELF
{

	using namespace LibELF;

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

	static BAN::ErrorOr<BAN::Vector<ElfNativeProgramHeader>> read_program_headers(BAN::RefPtr<Inode> inode, const ElfNativeFileHeader& file_header)
	{
		BAN::Vector<uint8_t> program_header_buffer;
		TRY(program_header_buffer.resize(file_header.e_phnum * file_header.e_phentsize));
		TRY(inode->read(file_header.e_phoff, BAN::ByteSpan(program_header_buffer.span())));

		BAN::Vector<ElfNativeProgramHeader> program_headers;
		TRY(program_headers.reserve(file_header.e_phnum));

		for (size_t i = 0; i < file_header.e_phnum; i++)
		{
			const auto& pheader = *reinterpret_cast<ElfNativeProgramHeader*>(program_header_buffer.data() + i * file_header.e_phentsize);
			if (pheader.p_memsz < pheader.p_filesz)
			{
				dprintln("Invalid program header, memsz less than filesz");
				return BAN::Error::from_errno(EINVAL);
			}

			MUST(program_headers.emplace_back(pheader));
		}

		return BAN::move(program_headers);
	}

	BAN::ErrorOr<LoadResult> load_from_inode(BAN::RefPtr<Inode> root, BAN::RefPtr<Inode> inode, const Credentials& credentials, PageTable& page_table)
	{
		auto file_header = TRY(read_and_validate_file_header(inode));
		auto program_headers = TRY(read_program_headers(inode, file_header));

		size_t exec_max_offset { 0 };
		BAN::String interpreter;

		for (const auto& program_header : program_headers)
		{
			if (program_header.p_type == PT_LOAD)
				exec_max_offset = BAN::Math::max<vaddr_t>(exec_max_offset, program_header.p_vaddr + program_header.p_memsz);
			else if (program_header.p_type == PT_INTERP)
			{
				BAN::Vector<uint8_t> interp_buffer;
				TRY(interp_buffer.resize(program_header.p_memsz, 0));
				TRY(inode->read(program_header.p_offset, BAN::ByteSpan(interp_buffer.data(), program_header.p_filesz)));
				if (interp_buffer.empty() || interp_buffer.front() != '/' || interp_buffer.back() != '\0')
				{
					dprintln("ELF interpreter is not an valid absolute path");
					return BAN::Error::from_errno(EINVAL);
				}

				auto interpreter_sv = BAN::StringView(reinterpret_cast<const char*>(interp_buffer.data()), interp_buffer.size() - 1);
				for (char ch : interpreter_sv)
				{
					if (isprint(ch))
						continue;
					dprintln("ELF interpreter name contains non-printable characters");
					return BAN::Error::from_errno(EINVAL);
				}

				interpreter.clear();
				TRY(interpreter.append(interpreter_sv));
			}
		}

		if (!interpreter.empty())
		{
			auto interpreter_inode = TRY(VirtualFileSystem::get().file_from_absolute_path(root, credentials, interpreter, O_EXEC)).inode;
			auto interpreter_file_header = TRY(read_and_validate_file_header(interpreter_inode));
			auto interpreter_program_headers = TRY(read_program_headers(interpreter_inode, interpreter_file_header));

			for (const auto& program_header : interpreter_program_headers)
			{
				if (program_header.p_type == PT_INTERP)
				{
					dprintln("ELF interpreter has an interpreter specified");
					return BAN::Error::from_errno(EINVAL);
				}
			}

			inode = interpreter_inode;
			file_header = interpreter_file_header;
			program_headers = BAN::move(interpreter_program_headers);
		}

		const vaddr_t load_base_vaddr =
			[&file_header, exec_max_offset]() -> vaddr_t
			{
				if (file_header.e_type == ET_EXEC)
					return 0;
				if (file_header.e_type == ET_DYN)
					return (exec_max_offset + PAGE_SIZE - 1) & PAGE_ADDR_MASK;
				ASSERT_NOT_REACHED();
			}();

		vaddr_t last_loaded_address = 0;
		BAN::Vector<BAN::UniqPtr<MemoryRegion>> memory_regions;
		for (const auto& program_header : program_headers)
		{
			if (program_header.p_type != PT_LOAD)
				continue;

			const PageTable::flags_t flags =
				[&program_header]() -> int
				{
					PageTable::flags_t result = PageTable::Flags::UserSupervisor;
					if (program_header.p_flags & PF_R)
						result |= PageTable::Flags::Present;
					if (program_header.p_flags & PF_W)
						result |= PageTable::Flags::ReadWrite;
					if (program_header.p_flags & PF_X)
						result |= PageTable::Flags::Execute;
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

			const vaddr_t pheader_base = load_base_vaddr + program_header.p_vaddr;

			if (file_backed_size)
			{
				auto region = TRY(FileBackedRegion::create(
					inode,
					page_table,
					program_header.p_offset,
					file_backed_size,
					{ .start = pheader_base, .end = pheader_base + file_backed_size },
					MemoryRegion::Type::PRIVATE,
					flags, O_EXEC | O_RDWR
				));
				TRY(memory_regions.emplace_back(BAN::move(region)));
			}

			if (file_backed_size < program_header.p_memsz)
			{
				const vaddr_t aligned_vaddr = pheader_base & PAGE_ADDR_MASK;

				auto region = TRY(MemoryBackedRegion::create(
					page_table,
					(pheader_base + program_header.p_memsz) - (aligned_vaddr + file_backed_size),
					{ .start = aligned_vaddr + file_backed_size, .end = pheader_base + program_header.p_memsz },
					MemoryRegion::Type::PRIVATE,
					flags, O_EXEC | O_RDWR
				));

				if (file_backed_size < program_header.p_filesz)
				{
					BAN::Vector<uint8_t> file_data_buffer;
					TRY(file_data_buffer.resize(program_header.p_filesz - file_backed_size));
					if (TRY(inode->read(program_header.p_offset + file_backed_size, file_data_buffer.span())) != file_data_buffer.size())
						return BAN::Error::from_errno(EFAULT);
					TRY(region->copy_data_to_region(pheader_base - aligned_vaddr, file_data_buffer.data(), file_data_buffer.size()));
				}

				TRY(memory_regions.emplace_back(BAN::move(region)));
			}

			last_loaded_address = BAN::Math::max(last_loaded_address, pheader_base + program_header.p_memsz);
		}

		LoadResult result;

		for (const auto& program_header : program_headers)
		{
			if (program_header.p_type != PT_TLS)
				continue;

			if (!BAN::Math::is_power_of_two(program_header.p_align))
				return BAN::Error::from_errno(EINVAL);

			size_t region_size = program_header.p_memsz;
			if (auto rem = region_size % program_header.p_align)
				region_size += program_header.p_align - rem;

			size_t offset = 0;
			if (auto rem = region_size % alignof(uthread))
				offset = alignof(uthread) - rem;

			auto region = TRY(MemoryBackedRegion::create(
				page_table,
				offset + region_size,
				{ .start = last_loaded_address, .end = USERSPACE_END },
				MemoryRegion::Type::PRIVATE,
				PageTable::Flags::UserSupervisor | PageTable::Flags::Present,
				O_EXEC | O_RDWR
			));

			for (vaddr_t vaddr = region->vaddr(); vaddr < region->vaddr() + offset + region->size(); vaddr += PAGE_SIZE)
				TRY(region->allocate_page_containing(vaddr, false));

			if (program_header.p_filesz > 0)
			{
				BAN::Vector<uint8_t> file_data_buffer;
				TRY(file_data_buffer.resize(program_header.p_filesz));
				if (TRY(inode->read(program_header.p_offset, file_data_buffer.span())) != file_data_buffer.size())
					return BAN::Error::from_errno(EFAULT);
				TRY(region->copy_data_to_region(offset, file_data_buffer.data(), file_data_buffer.size()));
			}

			result.master_tls = LoadResult::TLS {
				.addr = region->vaddr(),
				.size = region->size(),
			};

			TRY(memory_regions.emplace_back(BAN::move(region)));
		}

		result.open_execfd = !interpreter.empty();
		result.entry_point = load_base_vaddr + file_header.e_entry;
		result.regions = BAN::move(memory_regions);
		return BAN::move(result);
	}

}
