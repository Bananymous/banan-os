#include <BAN/StringView.h>
#include <kernel/Process.h>
#include <kernel/Scheduler.h>
#include <kernel/FS/VirtualFileSystem.h>

#include <fcntl.h>

namespace Kernel
{

	BAN::ErrorOr<BAN::RefPtr<Process>> Process::create_kernel(entry_t entry, void* data)
	{
		static pid_t next_pid = 1;
		auto process = TRY(BAN::RefPtr<Process>::create(next_pid++));
		TRY(process->m_working_directory.push_back('/'));
		TRY(process->add_thread(entry, data));
		return process;
	}

	BAN::ErrorOr<void> Process::add_thread(entry_t entry, void* data)
	{
		auto thread = TRY(Thread::create(entry, data, this));
		TRY(m_threads.push_back(thread));
		if (auto res = Scheduler::get().add_thread(thread); res.is_error())
		{
			m_threads.pop_back();
			return res;
		}

		return {};
	}

	void Process::on_thread_exit(Thread& thread)
	{
		(void)thread;
	}

	BAN::ErrorOr<int> Process::open(BAN::StringView path, int flags)
	{
		if (flags != O_RDONLY)
			return BAN::Error::from_errno(ENOTSUP);

		BAN::String absolute_path = TRY(absolute_path_of(path));

		auto file = TRY(VirtualFileSystem::get().file_from_absolute_path(absolute_path));

		int fd = TRY(get_free_fd());
		auto& open_file_description = m_open_files[fd];
		open_file_description.inode = file.inode;
		open_file_description.path = BAN::move(file.canonical_path);
		open_file_description.offset = 0;
		open_file_description.flags = flags;

		return fd;
	}

	BAN::ErrorOr<void> Process::close(int fd)
	{
		TRY(validate_fd(fd));
		auto& open_file_description = this->open_file_description(fd);
		open_file_description.inode = nullptr;
		return {};
	}

	BAN::ErrorOr<size_t> Process::read(int fd, void* buffer, size_t count)
	{
		TRY(validate_fd(fd));
		auto& open_file_description = this->open_file_description(fd);
		if (open_file_description.offset >= open_file_description.inode->size())
			return 0;
		size_t n_read = TRY(open_file_description.read(buffer, count));
		return n_read;
	}

	Inode& Process::inode_for_fd(int fd)
	{
		MUST(validate_fd(fd));
		return *open_file_description(fd).inode;
	}

	BAN::ErrorOr<void> Process::set_working_directory(BAN::StringView path)
	{
		BAN::String absolute_path = TRY(absolute_path_of(path));

		auto file = TRY(VirtualFileSystem::get().file_from_absolute_path(absolute_path));
		if (!file.inode->ifdir())
			return BAN::Error::from_errno(ENOTDIR);

		m_working_directory = BAN::move(file.canonical_path);

		return {};
	}

	BAN::ErrorOr<BAN::String> Process::absolute_path_of(BAN::StringView path) const
	{
		if (path.empty())
			return m_working_directory;
		BAN::String absolute_path;
		if (path.front() != '/')
		{
			TRY(absolute_path.append(m_working_directory));
			if (m_working_directory.sv().back() != '/')
				TRY(absolute_path.push_back('/'));
		}
		TRY(absolute_path.append(path));
		return absolute_path;
	}

	BAN::ErrorOr<size_t> Process::OpenFileDescription::read(void* buffer, size_t count)
	{
		size_t n_read = TRY(inode->read(offset, buffer, count));
		offset += n_read;
		return n_read;
	}

	BAN::ErrorOr<void> Process::validate_fd(int fd)
	{
		if (fd < 0 || m_open_files.size() <= (size_t)fd || !m_open_files[fd].inode)
			return BAN::Error::from_errno(EBADF);
		return {};
	}

	Process::OpenFileDescription& Process::open_file_description(int fd)
	{
		MUST(validate_fd(fd));
		return m_open_files[fd];
	}

	BAN::ErrorOr<int> Process::get_free_fd()
	{
		for (size_t fd = 0; fd < m_open_files.size(); fd++)
			if (!m_open_files[fd].inode)
				return fd;
		TRY(m_open_files.push_back({}));
		return m_open_files.size() - 1;
	}

}