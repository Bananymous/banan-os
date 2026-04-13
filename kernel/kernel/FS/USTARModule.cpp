#include <BAN/ScopeGuard.h>
#include <kernel/FS/USTARModule.h>
#include <kernel/Timer/Timer.h>
#include <LibDEFLATE/Decompressor.h>

#include <tar.h>

namespace Kernel
{

	class DataSource
	{
	public:
		DataSource() = default;
		virtual ~DataSource() = default;

		size_t data_size() const
		{
			return m_data_size;
		}

		BAN::ConstByteSpan data()
		{
			return { m_data_buffer, m_data_size };
		}

		void pop_data(size_t size)
		{
			ASSERT(size <= m_data_size);
			if (size > 0 && size < m_data_size)
				memmove(m_data_buffer, m_data_buffer + size, m_data_size - size);
			m_data_size -= size;
			m_bytes_produced += size;
		}

		virtual BAN::ErrorOr<bool> produce_data() = 0;

		uint64_t bytes_produced() const
		{
			return m_bytes_produced;
		}

		virtual uint64_t bytes_consumed() const = 0;

	protected:
		uint8_t m_data_buffer[4096];
		size_t m_data_size { 0 };

	private:
		uint64_t m_bytes_produced { 0 };
	};

	class DataSourceRaw final : public DataSource
	{
	public:
		DataSourceRaw(const BootModule& module)
			: m_module(module)
		{ }

		BAN::ErrorOr<bool> produce_data() override
		{
			if (m_offset >= m_module.size || m_data_size >= sizeof(m_data_buffer))
				return false;

			while (m_offset < m_module.size && m_data_size < sizeof(m_data_buffer))
			{
				const size_t to_copy = BAN::Math::min(
					sizeof(m_data_buffer) - m_data_size,
					PAGE_SIZE - (m_offset % PAGE_SIZE)
				);
				PageTable::with_fast_page((m_module.start + m_offset) & PAGE_ADDR_MASK, [&] {
					memcpy(m_data_buffer + m_data_size, PageTable::fast_page_as_ptr(m_offset % PAGE_SIZE), to_copy);
				});
				m_data_size += to_copy;
				m_offset += to_copy;
			}

			return true;
		}

		uint64_t bytes_consumed() const override
		{
			return bytes_produced();
		}

	private:
		const BootModule& m_module;
		size_t m_offset { 0 };
	};

	class DataSourceGZip final : public DataSource
	{
	public:
		DataSourceGZip(BAN::UniqPtr<DataSource>&& data_source)
			: m_data_source(BAN::move(data_source))
			, m_decompressor(LibDEFLATE::StreamType::GZip)
		{ }

		BAN::ErrorOr<bool> produce_data() override
		{
			if (m_is_done)
				return false;

			bool did_produce_data { false };
			for (;;)
			{
				TRY(m_data_source->produce_data());

				size_t input_consumed, output_produced;
				const auto status = TRY(m_decompressor.decompress(
					m_data_source->data(),
					input_consumed,
					{ m_data_buffer + m_data_size, sizeof(m_data_buffer) - m_data_size },
					output_produced
				));

				m_data_source->pop_data(input_consumed);
				m_data_size += output_produced;

				if (output_produced)
					did_produce_data = true;

				switch (status)
				{
					using DecompStatus = LibDEFLATE::Decompressor::Status;
					case DecompStatus::Done:
						m_is_done = true;
						return did_produce_data;
					case DecompStatus::NeedMoreInput:
						break;
					case DecompStatus::NeedMoreOutput:
						return did_produce_data;
				}
			}
		}

		uint64_t bytes_consumed() const override
		{
			return m_data_source->bytes_consumed();
		}

	private:
		BAN::UniqPtr<DataSource> m_data_source;
		LibDEFLATE::Decompressor m_decompressor;
		bool m_is_done { false };
	};

	static BAN::ErrorOr<void> unpack_boot_module_into_directory(BAN::RefPtr<Inode> root_inode, DataSource& data_source);

	BAN::ErrorOr<bool> unpack_boot_module_into_directory(BAN::RefPtr<Inode> root_inode, const BootModule& module)
	{
		ASSERT(root_inode->mode().ifdir());

		BAN::UniqPtr<DataSource> data_source = TRY(BAN::UniqPtr<DataSourceRaw>::create(module));

		bool is_compressed = false;

		TRY(data_source->produce_data());
		if (data_source->data_size() >= 2 && memcmp(&data_source->data()[0], "\x1F\x8B", 2) == 0)
		{
			data_source = TRY(BAN::UniqPtr<DataSourceGZip>::create(BAN::move(data_source)));
			is_compressed = true;
		}

		TRY(data_source->produce_data());
		if (data_source->data_size() < 512 || memcmp(&data_source->data()[257], "ustar", 5) != 0)
		{
			dwarnln("Unrecognized initrd format");
			return false;
		}

		const auto module_size_kib = module.size / 1024;
		dprintln("unpacking {}.{3} MiB{} initrd",
			module_size_kib / 1024, (module_size_kib % 1024) * 1000 / 1024,
			is_compressed ? " compressed" : ""
		);

		const auto unpack_ms1 = SystemTimer::get().ms_since_boot();
		TRY(unpack_boot_module_into_directory(root_inode, *data_source));
		const auto unpack_ms2 = SystemTimer::get().ms_since_boot();

		const auto duration_ms = unpack_ms2 - unpack_ms1;
		dprintln("unpacking {}.{3} MiB{} initrd took {}.{3} s",
			module_size_kib / 1024, (module_size_kib % 1024) * 1000 / 1024,
			is_compressed ? " compressed" : "",
			duration_ms / 1000, duration_ms % 1000
		);

		if (is_compressed)
		{
			const auto uncompressed_kib = data_source->bytes_produced() / 1024;
			dprintln("uncompressed size {}.{3} MiB",
				uncompressed_kib / 1024, (uncompressed_kib % 1024) * 1000 / 1024
			);
		}

		return true;
	}

	BAN::ErrorOr<void> unpack_boot_module_into_directory(BAN::RefPtr<Inode> root_inode, DataSource& data_source)
	{
		BAN::String next_file_name;
		BAN::String next_link_name;

		constexpr uint32_t print_interval_ms = 1000;
		auto next_print_ms = SystemTimer::get().ms_since_boot() + print_interval_ms;

		while (TRY(data_source.produce_data()), data_source.data_size() >= 512)
		{
			if (SystemTimer::get().ms_since_boot() >= next_print_ms)
			{
				const auto kib_consumed = data_source.bytes_consumed() / 1024;
				const auto kib_produced = data_source.bytes_produced() / 1024;
				if (kib_consumed == kib_produced)
				{
					dprintln(" ... {}.{3} MiB",
						kib_consumed / 1024, (kib_consumed % 1024) * 1000 / 1024
					);
				}
				else
				{
					dprintln(" ... {}.{3} MiB ({}.{3} MiB)",
						kib_consumed / 1024, (kib_consumed % 1024) * 1000 / 1024,
						kib_produced / 1024, (kib_produced % 1024) * 1000 / 1024
					);
				}
				next_print_ms = SystemTimer::get().ms_since_boot() + print_interval_ms;
			}

			const auto parse_octal =
				[&data_source](size_t offset, size_t length) -> size_t
				{
					size_t result = 0;
					for (size_t i = 0; i < length; i++)
					{
						const char ch = data_source.data()[offset + i];
						if (ch == '\0')
							break;
						result = (result * 8) + (ch - '0');
					}
					return result;
				};

			if (memcmp(&data_source.data()[257], "ustar", 5) != 0)
				break;

			char file_path[100 + 1 + 155 + 1];
			memcpy(file_path, &data_source.data()[345], 155);
			const size_t prefix_len = strlen(file_path);
			file_path[prefix_len] = '/';
			memcpy(file_path + prefix_len + 1, &data_source.data()[0], 100);

			mode_t        file_mode = parse_octal(100, 8);
			const uid_t   file_uid  = parse_octal(108, 8);
			const gid_t   file_gid  = parse_octal(116, 8);
			const size_t  file_size = parse_octal(124, 12);
			const uint8_t file_type = data_source.data()[156];

			auto parent_inode = root_inode;

			auto file_path_parts = TRY(BAN::StringView(next_file_name.empty() ? file_path : next_file_name.sv()).split('/'));
			for (size_t i = 0; i < file_path_parts.size() - 1; i++)
				parent_inode = TRY(parent_inode->find_inode(file_path_parts[i]));

			switch (file_type)
			{
				case 'L': case 'K': break;
				case REGTYPE:
				case AREGTYPE: file_mode |= Inode::Mode::IFREG; break;
				case LNKTYPE:                                   break;
				case SYMTYPE:  file_mode |= Inode::Mode::IFLNK; break;
				case CHRTYPE:  file_mode |= Inode::Mode::IFCHR; break;
				case BLKTYPE:  file_mode |= Inode::Mode::IFBLK; break;
				case DIRTYPE:  file_mode |= Inode::Mode::IFDIR; break;
				case FIFOTYPE: file_mode |= Inode::Mode::IFIFO; break;
				default:
					panic("unknown file type {}", file_type);
			}

			auto file_name_sv = file_path_parts.back();

			bool did_consume_data = false;

			if (file_type == 'L' || file_type == 'K')
			{
				auto& target_str = (file_type == 'L') ? next_file_name : next_link_name;
				TRY(target_str.resize(file_size));

				data_source.pop_data(512);

				size_t nwritten = 0;
				while (nwritten < file_size)
				{
					TRY(data_source.produce_data());
					if (data_source.data_size() == 0)
						return {};

					const size_t to_copy = BAN::Math::min(data_source.data_size(), file_size - nwritten);
					memcpy(target_str.data() + nwritten, data_source.data().data(), to_copy);
					nwritten += to_copy;

					data_source.pop_data(to_copy);
				}

				did_consume_data = true;

				while (!target_str.empty() && target_str.back() == '\0')
					target_str.pop_back();
			}
			else if (file_type == DIRTYPE)
			{
				if (file_name_sv == "."_sv)
					; // NOTE: don't create "." (root)
				else if (auto ret = parent_inode->create_directory(file_name_sv, file_mode, file_uid, file_gid); ret.is_error())
					dwarnln("failed to create directory '{}': {}", file_name_sv, ret.error());
			}
			else if (file_type == LNKTYPE)
			{
				BAN::StringView link_name;

				char link_buffer[101] {};
				if (!next_link_name.empty())
					link_name = next_link_name.sv();
				else
				{
					memcpy(link_buffer, &data_source.data()[157], 100);
					link_name = link_buffer;
				}

				auto target_inode = root_inode;

				auto link_path_parts = TRY(link_name.split('/'));
				for (const auto part : link_path_parts)
				{
					auto find_result = target_inode->find_inode(part);
					if (!find_result.is_error())
						target_inode = find_result.release_value();
					else
					{
						target_inode = {};
						break;
					}
				}

				if (target_inode)
					if (auto ret = parent_inode->link_inode(file_name_sv, target_inode); ret.is_error())
						dwarnln("failed to create hardlink '{}': {}", file_name_sv, ret.error());
			}
			else if (file_type == SYMTYPE)
			{
				if (auto ret = parent_inode->create_file(file_name_sv, file_mode, file_uid, file_gid); ret.is_error())
					dwarnln("failed to create symlink '{}': {}", file_name_sv, ret.error());
				else
				{
					BAN::StringView link_name;

					char link_buffer[101] {};
					if (!next_link_name.empty())
						link_name = next_link_name.sv();
					else
					{
						memcpy(link_buffer, &data_source.data()[157], 100);
						link_name = link_buffer;
					}

					auto inode = TRY(parent_inode->find_inode(file_name_sv));
					TRY(inode->set_link_target(link_name));
				}
			}
			else
			{
				if (auto ret = parent_inode->create_file(file_name_sv, file_mode, file_uid, file_gid); ret.is_error())
					dwarnln("failed to create file '{}': {}", file_name_sv, ret.error());
				else if (file_size)
				{
					auto inode = TRY(parent_inode->find_inode(file_name_sv));

					data_source.pop_data(512);

					size_t nwritten = 0;
					while (nwritten < file_size)
					{
						TRY(data_source.produce_data());
						ASSERT(data_source.data_size() > 0); // what to do?

						const size_t to_write = BAN::Math::min(file_size - nwritten, data_source.data_size());
						TRY(inode->write(nwritten, data_source.data().slice(0, to_write)));
						nwritten += to_write;

						data_source.pop_data(to_write);
					}

					did_consume_data = true;
				}
			}

			if (file_type != 'L' && file_type != 'K')
			{
				next_file_name.clear();
				next_link_name.clear();
			}

			if (!did_consume_data)
			{
				data_source.pop_data(512);

				size_t consumed = 0;
				while (consumed < file_size)
				{
					TRY(data_source.produce_data());
					if (data_source.data_size() == 0)
						return {};
					data_source.pop_data(BAN::Math::min(file_size - consumed, data_source.data_size()));
				}
			}

			if (const auto rem = file_size % 512)
			{
				TRY(data_source.produce_data());
				if (data_source.data_size() < rem)
					return {};
				data_source.pop_data(512 - rem);
			}
		}

		return {};
	}

}
