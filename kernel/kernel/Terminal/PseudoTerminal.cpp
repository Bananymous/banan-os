#include <kernel/Device/DeviceNumbers.h>
#include <kernel/FS/DevFS/FileSystem.h>
#include <kernel/Terminal/PseudoTerminal.h>

#include <BAN/ScopeGuard.h>

#include <sys/epoll.h>
#include <sys/ioctl.h>
#include <sys/sysmacros.h>

namespace Kernel
{

	static BAN::Atomic<uint32_t> s_pts_master_minor = 0;

	static SpinLock s_pts_slave_bitmap_lock;
	static uint64_t s_pts_slave_bitmap = 0;

	BAN::ErrorOr<BAN::RefPtr<PseudoTerminalMaster>> PseudoTerminalMaster::create(mode_t mode, uid_t uid, gid_t gid)
	{
		size_t pts_slave_number = 0;
		{
			SpinLockGuard _(s_pts_slave_bitmap_lock);
			for (pts_slave_number = 0; pts_slave_number < sizeof(s_pts_slave_bitmap) * 8; pts_slave_number++)
			{
				if (s_pts_slave_bitmap & ((uint64_t)1 << pts_slave_number))
					continue;
				s_pts_slave_bitmap |= (uint64_t)1 << pts_slave_number;
				break;
			}
		}
		if (pts_slave_number >= sizeof(s_pts_slave_bitmap) * 8)
			return BAN::Error::from_errno(ENODEV);
		BAN::ScopeGuard slave_number_clearer(
			[pts_slave_number]()
			{
				SpinLockGuard _(s_pts_slave_bitmap_lock);
				s_pts_slave_bitmap &= ~((uint64_t)1 << pts_slave_number);
			}
		);

		auto pts_master_buffer = TRY(VirtualRange::create_to_vaddr_range(
			PageTable::kernel(),
			KERNEL_OFFSET, static_cast<vaddr_t>(-1),
			16 * PAGE_SIZE,
			PageTable::Flags::ReadWrite | PageTable::Flags::Present, true
		));
		auto pts_master = TRY(BAN::RefPtr<PseudoTerminalMaster>::create(BAN::move(pts_master_buffer), mode, uid, gid));
		DevFileSystem::get().remove_from_cache(pts_master);

		auto pts_slave_name = TRY(BAN::String::formatted("pts{}", pts_slave_number));
		auto pts_slave = TRY(BAN::RefPtr<PseudoTerminalSlave>::create(BAN::move(pts_slave_name), pts_slave_number, 0610, uid, gid));
		slave_number_clearer.disable();

		pts_master->m_slave = TRY(pts_slave->get_weak_ptr());
		pts_slave->m_master = TRY(pts_master->get_weak_ptr());

		DevFileSystem::get().add_device(pts_slave);

		return pts_master;
	}

	PseudoTerminalMaster::PseudoTerminalMaster(BAN::UniqPtr<VirtualRange> buffer, mode_t mode, uid_t uid, gid_t gid)
		: CharacterDevice(mode, uid, gid)
		, m_buffer(BAN::move(buffer))
		, m_rdev(makedev(DeviceNumber::PTSMaster, s_pts_master_minor++))
	{ }

	PseudoTerminalMaster::~PseudoTerminalMaster()
	{
		if (auto slave = m_slave.lock())
			DevFileSystem::get().remove_device(slave);
	}

	BAN::ErrorOr<BAN::RefPtr<PseudoTerminalSlave>> PseudoTerminalMaster::slave()
	{
		if (auto slave = m_slave.lock())
			return slave;
		return BAN::Error::from_errno(ENODEV);
	}

	BAN::ErrorOr<BAN::String> PseudoTerminalMaster::ptsname()
	{
		if (auto slave = m_slave.lock())
			return TRY(BAN::String::formatted("/dev/{}", slave->name()));
		return BAN::Error::from_errno(ENODEV);
	}

	bool PseudoTerminalMaster::putchar(uint8_t ch)
	{
		{
			SpinLockGuard _(m_buffer_lock);

			if (m_buffer_size >= m_buffer->size())
				return false;

			reinterpret_cast<uint8_t*>(m_buffer->vaddr())[(m_buffer_tail + m_buffer_size) % m_buffer->size()] = ch;
			m_buffer_size++;

			m_buffer_blocker.unblock();
		}

		epoll_notify(EPOLLIN);

		return true;
	}

	BAN::ErrorOr<size_t> PseudoTerminalMaster::read_impl(off_t, BAN::ByteSpan buffer)
	{
		auto state = m_buffer_lock.lock();

		while (m_buffer_size == 0)
		{
			m_buffer_lock.unlock(state);
			TRY(Thread::current().block_or_eintr_indefinite(m_buffer_blocker));
			m_buffer_lock.lock();
		}

		const size_t to_copy = BAN::Math::min(buffer.size(), m_buffer_size);

		if (m_buffer_tail + to_copy <= m_buffer->size())
			memcpy(buffer.data(), reinterpret_cast<void*>(m_buffer->vaddr() + m_buffer_tail), to_copy);
		else
		{
			const size_t before_wrap = m_buffer->size() - m_buffer_tail;
			const size_t after_wrap = to_copy - before_wrap;

			memcpy(buffer.data(), reinterpret_cast<void*>(m_buffer->vaddr() + m_buffer_tail), before_wrap);
			memcpy(buffer.data() + before_wrap, reinterpret_cast<void*>(m_buffer->vaddr()), after_wrap);
		}

		m_buffer_size -= to_copy;
		m_buffer_tail = (m_buffer_tail + to_copy) % m_buffer->size();

		m_buffer_lock.unlock(state);

		epoll_notify(EPOLLOUT);

		return to_copy;
	}

	BAN::ErrorOr<size_t> PseudoTerminalMaster::write_impl(off_t, BAN::ConstByteSpan buffer)
	{
		auto slave = m_slave.lock();
		if (!slave)
			return BAN::Error::from_errno(ENODEV);
		for (size_t i = 0; i < buffer.size(); i++)
			slave->handle_input_byte(buffer[i]);
		return buffer.size();
	}

	BAN::ErrorOr<long> PseudoTerminalMaster::ioctl_impl(int request, void* argument)
	{
		auto slave = m_slave.lock();
		if (!slave)
			return BAN::Error::from_errno(ENODEV);
		return slave->ioctl(request, argument);
	}

	PseudoTerminalSlave::PseudoTerminalSlave(BAN::String&& name, uint32_t number, mode_t mode, uid_t uid, gid_t gid)
		: TTY({
			.c_iflag = 0,
			.c_oflag = 0,
			.c_cflag = CS8,
			.c_lflag = ECHO | ICANON,
			.c_cc = {},
			.c_ospeed = B38400,
			.c_ispeed = B38400,
		  }, mode, uid, gid)
		, m_name(BAN::move(name))
		, m_number(number)
	{}

	PseudoTerminalSlave::~PseudoTerminalSlave()
	{
		SpinLockGuard _(s_pts_slave_bitmap_lock);
		s_pts_slave_bitmap &= ~((uint64_t)1 << m_number);
	}

	void PseudoTerminalSlave::clear()
	{
		const char message[] { '\e', '[', '2', 'J' };
		(void)write_impl(0, BAN::ConstByteSpan::from(message));
	}

	bool PseudoTerminalSlave::putchar_impl(uint8_t ch)
	{
		if (auto master = m_master.lock())
			return master->putchar(ch);
		return false;
	}

	BAN::ErrorOr<long> PseudoTerminalSlave::ioctl_impl(int request, void* argument)
	{
		switch (request)
		{
			case TIOCSWINSZ:
			{
				const auto* winsize = static_cast<struct winsize*>(argument);
				m_width = winsize->ws_col;
				m_height = winsize->ws_row;
				return 0;
			}
		}

		return TTY::ioctl_impl(request, argument);
	}

}
