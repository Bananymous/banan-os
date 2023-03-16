#pragma once

#include <BAN/String.h>
#include <BAN/Vector.h>
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

		static BAN::RefPtr<Process> current() { return Thread::current()->process(); }

	private:
		Process(pid_t pid) : m_pid(pid) {}

	private:
		pid_t m_pid = 0;
		BAN::String m_working_directory;
		BAN::Vector<BAN::RefPtr<Thread>> m_threads;

		friend class BAN::RefPtr<Process>;
	};

}