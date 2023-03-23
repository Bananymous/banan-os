#pragma once

#include <BAN/String.h>
#include <BAN/StringView.h>
#include <BAN/Vector.h>
#include <kernel/FS/Inode.h>
#include <kernel/SpinLock.h>
#include <kernel/Thread.h>

namespace Kernel
{

	class Process : BAN::RefCounted<Process>
	{
		BAN_NON_COPYABLE(Process);
		BAN_NON_MOVABLE(Process);

	public:
		using entry_t = Thread::entry_t;

	public:
		static BAN::ErrorOr<BAN::RefPtr<Process>> create_kernel(entry_t, void*);
		~Process() {}

		BAN::ErrorOr<void> add_thread(entry_t, void*);
		void on_thread_exit(Thread&);

		pid_t pid() const { return m_pid; }

		BAN::ErrorOr<int> open(BAN::StringView, int);
		BAN::ErrorOr<void> close(int);
		BAN::ErrorOr<size_t> read(int, void*, size_t);
		BAN::ErrorOr<void> creat(BAN::StringView, mode_t);

		BAN::String working_directory() const;
		BAN::ErrorOr<void> set_working_directory(BAN::StringView);

		Inode& inode_for_fd(int);

		static BAN::RefPtr<Process> current() { return Thread::current()->process(); }

	private:
		Process(pid_t pid) : m_pid(pid) {}

		BAN::ErrorOr<BAN::String> absolute_path_of(BAN::StringView) const;

	private:
		struct OpenFileDescription
		{
			BAN::RefPtr<Inode> inode;
			BAN::String path;
			size_t offset = 0;
			uint8_t flags = 0;

			BAN::ErrorOr<size_t> read(void*, size_t);
		};

		BAN::ErrorOr<void> validate_fd(int);
		OpenFileDescription& open_file_description(int);
		BAN::ErrorOr<int> get_free_fd();

		BAN::Vector<OpenFileDescription> m_open_files;

		mutable RecursiveSpinLock m_lock;

		const pid_t m_pid = 0;
		BAN::String m_working_directory;
		BAN::Vector<BAN::RefPtr<Thread>> m_threads;

		friend class BAN::RefPtr<Process>;
	};

}