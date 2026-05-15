#include <BAN/Errors.h>
#include <BAN/ScopeGuard.h>
#include <BAN/UTF8.h>
#include <kernel/Debug.h>
#include <kernel/Device/DeviceNumbers.h>
#include <kernel/FS/DevFS/FileSystem.h>
#include <kernel/FS/VirtualFileSystem.h>
#include <kernel/Lock/LockGuard.h>
#include <kernel/Lock/SpinLockAsMutex.h>
#include <kernel/Process.h>
#include <kernel/Terminal/TTY.h>
#include <kernel/Timer/Timer.h>
#include <LibInput/KeyboardLayout.h>

#include <fcntl.h>
#include <string.h>
#include <stropts.h>
#include <sys/banan-os.h>
#include <sys/epoll.h>
#include <sys/sysmacros.h>

#define NL '\n'
#define CR '\r'

namespace Kernel
{

	class DevTTY : public TmpInode
	{
	public:
		static BAN::ErrorOr<BAN::RefPtr<DevTTY>> create(mode_t mode, uid_t uid, gid_t gid)
		{
			return TRY(BAN::RefPtr<DevTTY>::create(mode | Inode::Mode::IFLNK, uid, gid));
		}

	protected:
		BAN::ErrorOr<BAN::String> link_target_impl() override
		{
			auto terminal = Process::current().controlling_terminal();
			if (!terminal)
				return BAN::Error::from_errno(ENODEV);
			return TRY(BAN::String::formatted("/dev/{}", terminal->name()));
		}

		bool can_read_impl() const override { return false; }
		bool can_write_impl() const override { return false; }
		bool has_error_impl() const override { return false; }
		bool has_hungup_impl() const override { return false; }

	private:
		DevTTY(mode_t mode, uid_t uid, gid_t gid)
			: TmpInode(
				DevFileSystem::get(),
				MUST(DevFileSystem::get().allocate_inode(create_inode_info(mode, uid, gid))),
				create_inode_info(mode, uid, gid)
			)
		{
			ASSERT(this->mode().iflnk());
		}

	private:
		friend class BAN::RefPtr<DevTTY>;
	};

	static BAN::RefPtr<TTY> s_tty;

	static dev_t next_tty_rdev()
	{
		static BAN::Atomic<dev_t> s_minor = 0;
		return makedev(DeviceNumber::TTY, s_minor++);
	}

	TTY::TTY(termios termios, mode_t mode, uid_t uid, gid_t gid)
		: CharacterDevice(mode, uid, gid)
		, m_termios(termios)
	{
		m_rdev = next_tty_rdev();
		m_output.buffer = MUST(ByteRingBuffer::create(PAGE_SIZE));
	}

	BAN::RefPtr<TTY> TTY::current()
	{
		ASSERT(s_tty);
		return s_tty;
	}

	void TTY::set_as_current()
	{
		s_tty = this;
		clear();
	}

	BAN::ErrorOr<void> TTY::tty_ctrl(int command, int flags)
	{
		if (flags & ~(TTY_FLAG_ENABLE_INPUT | TTY_FLAG_ENABLE_OUTPUT))
			return BAN::Error::from_errno(EINVAL);

		switch (command)
		{
			case TTY_CMD_SET:
				if (flags & TTY_FLAG_ENABLE_INPUT)
					m_tty_ctrl.receive_input = true;
				if (flags & TTY_FLAG_ENABLE_OUTPUT)
					m_tty_ctrl.draw_graphics = true;
				break;
			case TTY_CMD_UNSET:
				if (flags & TTY_FLAG_ENABLE_INPUT)
					m_tty_ctrl.receive_input = false;
				if (flags & TTY_FLAG_ENABLE_OUTPUT)
					m_tty_ctrl.draw_graphics = false;
				break;
			default:
				return BAN::Error::from_errno(EINVAL);
		}

		return {};
	}

	void TTY::keyboard_task(void*)
	{
		BAN::RefPtr<Inode> keyboard_inode;
		if (auto ret = DevFileSystem::get().root_inode()->find_inode("keyboard"_sv); !ret.is_error())
			keyboard_inode = ret.release_value();
		else
		{
			dprintln("could not open keyboard device: {}", ret.error());
			return;
		}

		while (true)
		{
			while (TTY::current()->m_tty_ctrl.receive_input)
			{
				if (!keyboard_inode->can_read())
				{
					SystemTimer::get().sleep_ms(1);
					continue;
				}

				LibInput::RawKeyEvent event;
				[[maybe_unused]] const size_t read = MUST(keyboard_inode->read(0, BAN::ByteSpan::from(event)));
				ASSERT(read == sizeof(event));

				TTY::current()->on_key_event(LibInput::KeyboardLayout::get().key_event_from_raw(event));
			}
		}
	}

	void TTY::initialize_devices()
	{
		auto* thread = MUST(Thread::create_kernel(&TTY::keyboard_task, nullptr));
		MUST(Processor::scheduler().add_thread(thread));

		DevFileSystem::get().add_inode("tty", MUST(DevTTY::create(0666, 0, 0)));
	}

	BAN::ErrorOr<void> TTY::chmod_impl(mode_t mode)
	{
		// FIXME: make this atomic
		ASSERT((mode & Inode::Mode::TYPE_MASK) == 0);
		m_mode &= Inode::Mode::TYPE_MASK;
		m_mode |= mode;
		return {};
	}

	BAN::ErrorOr<void> TTY::chown_impl(uid_t uid, gid_t gid)
	{
		m_uid = uid;
		m_gid = gid;
		return {};
	}

	void TTY::update_winsize(unsigned short cols, unsigned short rows)
	{
		// FIXME: make this atomic
		m_winsize.ws_col = cols;
		m_winsize.ws_row = rows;
		(void)Process::kill(-m_foreground_pgrp, SIGWINCH);
	}

	BAN::ErrorOr<long> TTY::ioctl_impl(int request, void* argument)
	{
		switch (request)
		{
			case KDLOADFONT:
			{
				auto absolute_path = TRY(Process::current().absolute_path_of(BAN::StringView(reinterpret_cast<const char*>(argument))));
				auto new_font = TRY(LibFont::Font::load(absolute_path));
				TRY(set_font(BAN::move(new_font)));
				return 0;
			}
			case FIONREAD:
			{
				*static_cast<int*>(argument) = m_output.flush ? m_output.buffer->size() : 0;
				return 0;
			}
			case TIOCGWINSZ:
			{
				// FIXME: make this atomic
				auto* winsize = static_cast<struct winsize*>(argument);
				*winsize = m_winsize;
				return 0;
			}
			case TIOCSWINSZ:
			{
				// FIXME: make this atomic
				const auto* winsize = static_cast<const struct winsize*>(argument);
				m_winsize = *winsize;
				(void)Process::kill(-m_foreground_pgrp, SIGWINCH);
				return 0;
			}
			case TCGETS:
			{
				SpinLockGuard _(m_termios_lock);
				auto* termios = static_cast<struct termios*>(argument);
				*termios = m_termios;
				return 0;
			}
			case TCSETSW:
			case TCSETSF:
				dwarnln("TODO: proper TCSETSW/TCSETSWF");
				[[fallthrough]];
			case TCSETS:
			{
				// FIXME: do some validation
				SpinLockGuard _(m_termios_lock);
				const auto* termios = static_cast<const struct termios*>(argument);
				m_termios = *termios;
				return 0;
			}
			case TIOCGPGRP:
			{
				pid_t* pgrp = static_cast<pid_t*>(argument);
				*pgrp = m_foreground_pgrp.load();
				return 0;
			}
			case TIOCSPGRP:
			{
				const pid_t pgrp = *static_cast<const pid_t*>(argument);

				if (!Process::current().is_pgrpg_in_this_session(pgrp))
					return BAN::Error::from_errno(EPERM);

				if (this != Process::current().controlling_terminal().ptr())
					return BAN::Error::from_errno(ENOTTY);

				m_foreground_pgrp = pgrp;
				return 0;
			}
		}

		return CharacterDevice::ioctl(request, argument);
	}

	void TTY::on_key_event(LibInput::RawKeyEvent event)
	{
		on_key_event(LibInput::KeyboardLayout::get().key_event_from_raw(event));
	}

	void TTY::on_key_event(LibInput::KeyEvent event)
	{
		if (event.released())
			return;

		const char* ansi_c_str = LibInput::key_to_utf8_ansi(event.key, event.modifier);
		if (ansi_c_str == nullptr)
			return;

		LockGuard _(m_mutex);
		while (*ansi_c_str)
			handle_input_byte(*ansi_c_str++);
		after_write();
	}

	termios TTY::get_termios()
	{
		SpinLockGuard _(m_termios_lock);
		return m_termios;
	}

	void TTY::handle_input_byte(uint8_t ch)
	{
		if (ch == _POSIX_VDISABLE)
			return;

		LockGuard _0(m_mutex);

		const auto termios = get_termios();

		if ((termios.c_iflag & ISTRIP))
			ch &= 0x7F;
		if ((termios.c_iflag & IGNCR) && ch == CR)
			return;
		if ((termios.c_iflag & ICRNL) && ch == CR)
			ch = NL;
		else if ((termios.c_iflag & INLCR) && ch == NL)
			ch = CR;

		if (termios.c_lflag & ISIG)
		{
			int sig = -1;
			if (ch == termios.c_cc[VINTR])
				sig = SIGINT;
			if (ch == termios.c_cc[VQUIT])
				sig = SIGQUIT;
			if (ch == termios.c_cc[VSUSP])
				sig = SIGTSTP;
			if (sig != -1)
			{
				if (auto ret = Process::kill(-m_foreground_pgrp, sig); ret.is_error())
					dwarnln("TTY: {}", ret.error());
				return;
			}
		}

		bool should_append = true;
		bool should_flush = false;
		bool force_echo = false;

		if (!(termios.c_lflag & ICANON))
			should_flush = true;
		else
		{
			if (ch == termios.c_cc[VERASE] && (termios.c_lflag & ECHOE))
				return do_backspace();

			if (ch == termios.c_cc[VKILL] && (termios.c_lflag & ECHOK))
			{
				while (!m_output.buffer->empty() && m_output.buffer->back() != '\n')
					do_backspace();
				return;
			}

			if (ch == termios.c_cc[VEOF])
			{
				should_append = false;
				should_flush = true;
			}

			if (ch == NL || ch == CR || ch == termios.c_cc[VEOL])
			{
				should_flush = true;
				force_echo = !!(termios.c_lflag & ECHONL);
			}
		}

		// TODO: terminal suspension with VSTOP/VSTART

		if (should_append && !m_output.buffer->full())
			m_output.buffer->push({ &ch, 1 });

		if (should_append && (force_echo || (termios.c_lflag & ECHO)))
		{
			if ((ch <= 31 || ch == 127) && ch != '\n')
			{
				putchar('^');
				if (ch <= 26 && ch != 10)
					putchar('A' + ch - 1);
				else if (ch == 27)
					putchar('[');
				else if (ch == 28)
					putchar('\\');
				else if (ch == 29)
					putchar(']');
				else if (ch == 30)
					putchar('^');
				else if (ch == 31)
					putchar('_');
				else if (ch == 127)
					putchar('?');
			}
			else
			{
				putchar(ch);
			}
		}

		if (should_flush)
		{
			m_output.flush = true;
			epoll_notify(EPOLLIN);
			m_output.thread_blocker.unblock();
		}
	}

	void TTY::do_backspace()
	{
		if (m_output.buffer->empty())
			return;

		const bool is_caret_notation =
			(m_output.buffer->back() < 32) ||
			(m_output.buffer->back() == 127);

		// handle multibyte UTF8
		while ((m_output.buffer->back() & 0xC0) == 0x80)
			m_output.buffer->pop_back(1);

		ASSERT(!m_output.buffer->empty());
		m_output.buffer->pop_back(1);

		if (is_caret_notation)
		{
			putchar('\b');
			putchar('\b');
			putchar(' ');
			putchar(' ');
			putchar('\b');
			putchar('\b');
		}
		else
		{
			putchar('\b');
			putchar(' ');
			putchar('\b');
		}
	}

	bool TTY::putchar(uint8_t ch)
	{
		if (!m_tty_ctrl.draw_graphics)
			return true;

		const auto termios = get_termios();

		SpinLockGuard _1(m_write_lock);
		if (termios.c_oflag & OPOST)
		{
			if ((termios.c_oflag & ONLCR) && ch == NL)
				return putchar_impl(CR) && putchar_impl(NL);
			if ((termios.c_oflag & OCRNL) && ch == CR)
				return putchar_impl(NL);
		}
		return putchar_impl(ch);
	}

	BAN::ErrorOr<size_t> TTY::read_impl(off_t, BAN::ByteSpan buffer)
	{
		LockGuard _(m_mutex);
		while (!m_output.flush)
			TRY(Thread::current().block_or_eintr_indefinite(m_output.thread_blocker, &m_mutex));

		if (m_output.buffer->empty())
		{
			if (master_has_closed())
				return 0;
			m_output.flush = false;
			return 0;
		}

		auto data = m_output.buffer->get_data();

		const size_t max_to_copy = BAN::Math::min<size_t>(buffer.size(), data.size());
		size_t to_copy = max_to_copy;
		if (m_termios.c_lflag & ICANON)
			for (to_copy = 1; to_copy < max_to_copy; to_copy++)
				if (data[to_copy - 1] == NL)
					break;

		memcpy(buffer.data(), data.data(), to_copy);
		m_output.buffer->pop(to_copy);

		if (m_output.buffer->empty())
			m_output.flush = false;

		m_output.thread_blocker.unblock();

		return to_copy;
	}

	BAN::ErrorOr<size_t> TTY::write_impl(off_t, BAN::ConstByteSpan buffer)
	{
		SpinLockGuard write_guard(m_write_lock);

		while (!can_write())
		{
			if (master_has_closed())
				return BAN::Error::from_errno(EIO);

			SpinLockGuardAsMutex smutex(write_guard);
			TRY(Thread::current().block_or_eintr_indefinite(m_write_blocker, &smutex));
		}

		size_t written = 0;
		for (; written < buffer.size(); written++)
			if (!putchar(buffer[written]))
				break;
		after_write();

		if (can_write())
			epoll_notify(EPOLLOUT);

		return written;
	}

	void TTY::putchar_current(uint8_t ch)
	{
		ASSERT(s_tty);
		SpinLockGuard _(s_tty->m_write_lock);
		s_tty->putchar(ch);
		s_tty->after_write();
	}

	bool TTY::is_initialized()
	{
		return !!s_tty;
	}

}
