#include <BAN/Errors.h>
#include <BAN/ScopeGuard.h>
#include <BAN/UTF8.h>
#include <kernel/Debug.h>
#include <kernel/Device/DeviceNumbers.h>
#include <kernel/FS/DevFS/FileSystem.h>
#include <kernel/FS/VirtualFileSystem.h>
#include <kernel/Lock/LockGuard.h>
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
		, m_rdev(next_tty_rdev())
	{ }

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

		LockGuard _(m_mutex);

		switch (command)
		{
			case TTY_CMD_SET:
				if ((flags & TTY_FLAG_ENABLE_INPUT) && !m_tty_ctrl.receive_input)
				{
					m_tty_ctrl.receive_input = true;
					m_tty_ctrl.thread_blocker.unblock();
				}
				if (flags & TTY_FLAG_ENABLE_OUTPUT)
					m_tty_ctrl.draw_graphics = true;
				break;
			case TTY_CMD_UNSET:
				if ((flags & TTY_FLAG_ENABLE_INPUT) && m_tty_ctrl.receive_input)
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
		if (auto ret = VirtualFileSystem::get().file_from_absolute_path({ 0, 0, 0, 0 }, "/dev/keyboard"_sv, O_RDONLY); !ret.is_error())
			keyboard_inode = ret.value().inode;
		else
		{
			dprintln("could not open keyboard device: {}", ret.error());
			return;
		}

		while (true)
		{
			{
				LockGuard _(TTY::current()->m_mutex);
				while (!TTY::current()->m_tty_ctrl.receive_input)
					TTY::current()->m_tty_ctrl.thread_blocker.block_indefinite(&TTY::current()->m_mutex);
			}

			while (TTY::current()->m_tty_ctrl.receive_input)
			{
				LockGuard _(keyboard_inode->m_mutex);

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
		static bool initialized = false;
		ASSERT(!initialized);

		auto* thread = MUST(Thread::create_kernel(&TTY::keyboard_task, nullptr));
		MUST(Processor::scheduler().add_thread(thread));

		DevFileSystem::get().add_inode("tty", MUST(DevTTY::create(0666, 0, 0)));

		initialized = true;
	}

	BAN::ErrorOr<void> TTY::chmod_impl(mode_t mode)
	{
		ASSERT((mode & Inode::Mode::TYPE_MASK) == 0);
		m_inode_info.mode &= Inode::Mode::TYPE_MASK;
		m_inode_info.mode |= mode;
		return {};
	}

	BAN::ErrorOr<void> TTY::chown_impl(uid_t uid, gid_t gid)
	{
		m_inode_info.uid = uid;
		m_inode_info.gid = gid;
		return {};
	}

	void TTY::update_winsize(unsigned short cols, unsigned short rows)
	{
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
			case TIOCGWINSZ:
			{
				auto* winsize = static_cast<struct winsize*>(argument);
				*winsize = m_winsize;
				return 0;
			}
			case TIOCSWINSZ:
			{
				const auto* winsize = static_cast<const struct winsize*>(argument);
				m_winsize = *winsize;
				(void)Process::kill(-m_foreground_pgrp, SIGWINCH);
				return 0;
			}
		}
		return BAN::Error::from_errno(ENOTSUP);
	}

	void TTY::on_key_event(LibInput::KeyEvent event)
	{
		LockGuard _(m_mutex);

		if (event.released())
			return;

		const char* ansi_c_str = LibInput::key_to_utf8_ansi(event.key, event.modifier);
		if (ansi_c_str)
		{
			auto* ptr = reinterpret_cast<const uint8_t*>(ansi_c_str);
			while (*ptr)
				handle_input_byte(*ptr++);
			after_write();
		}
	}

	void TTY::handle_input_byte(uint8_t ch)
	{
		if (ch == _POSIX_VDISABLE)
			return;

		LockGuard _(m_mutex);

		if (m_termios.c_lflag & ICANON)
		{
			if ((m_termios.c_iflag & ISTRIP))
				ch &= 0x7F;
			if ((m_termios.c_iflag & IGNCR) && ch == CR)
				return;
			uint8_t conv = ch;
			if ((m_termios.c_iflag & ICRNL) && ch == CR)
				conv = NL;
			if ((m_termios.c_iflag & INLCR) && ch == NL)
				conv = CR;
			ch = conv;
		}

		if (m_termios.c_lflag & ISIG)
		{
			int sig = -1;
			if (ch == m_termios.c_cc[VINTR])
				sig = SIGINT;
			if (ch == m_termios.c_cc[VQUIT])
				sig = SIGQUIT;
			if (ch == m_termios.c_cc[VSUSP])
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

		if (!(m_termios.c_lflag & ICANON))
			should_flush = true;
		else
		{
			if (ch == m_termios.c_cc[VERASE] && (m_termios.c_lflag & ECHOE))
				return do_backspace();

			if (ch == m_termios.c_cc[VKILL] && (m_termios.c_lflag & ECHOK))
			{
				while (m_output.bytes > 0 && m_output.buffer[m_output.bytes - 1] != '\n')
					do_backspace();
				return;
			}

			if (ch == m_termios.c_cc[VEOF])
			{
				should_append = false;
				should_flush = true;
			}

			if (ch == NL || ch == m_termios.c_cc[VEOL])
			{
				should_flush = true;
				force_echo = !!(m_termios.c_lflag & ECHONL);
				ch = NL;
			}
		}

		// TODO: terminal suspension with VSTOP/VSTART

		if (should_append)
		{
			if (m_output.bytes >= m_output.buffer.size())
				return;
			m_output.buffer[m_output.bytes++] = ch;
		}

		if (should_append && (force_echo || (m_termios.c_lflag & ECHO)))
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
		if (m_output.bytes > 0)
		{
			uint8_t last = m_output.buffer[m_output.bytes - 1];

			// Multibyte UTF8
			if ((last & 0xC0) == 0x80)
			{
				// NOTE: this should be valid UTF8 since keyboard input already 'validates' it
				while ((m_output.buffer[m_output.bytes - 1] & 0xC0) == 0x80)
				{
					ASSERT(m_output.bytes > 0);
					m_output.bytes--;
				}
				ASSERT(m_output.bytes > 0);
				m_output.bytes--;
				putchar('\b');
				putchar(' ');
				putchar('\b');
			}
			// Caret notation
			else if (last < 32 || last == 127)
			{
				m_output.bytes--;
				putchar('\b');
				putchar('\b');
				putchar(' ');
				putchar(' ');
				putchar('\b');
				putchar('\b');
			}
			// Ascii
			else
			{
				m_output.bytes--;
				putchar('\b');
				putchar(' ');
				putchar('\b');
			}
		}
	}

	bool TTY::putchar(uint8_t ch)
	{
		SpinLockGuard _(m_write_lock);
		if (!m_tty_ctrl.draw_graphics)
			return true;
		if (m_termios.c_oflag & OPOST)
		{
			if ((m_termios.c_oflag & ONLCR) && ch == NL)
				return putchar_impl(CR) && putchar_impl(NL);
			if ((m_termios.c_oflag & OCRNL) && ch == CR)
				return putchar_impl(NL);
		}
		return putchar_impl(ch);
	}

	BAN::ErrorOr<size_t> TTY::read_impl(off_t, BAN::ByteSpan buffer)
	{
		while (!m_output.flush)
			TRY(Thread::current().block_or_eintr_indefinite(m_output.thread_blocker, &m_mutex));

		if (m_output.bytes == 0)
		{
			if (master_has_closed())
				return 0;
			m_output.flush = false;
			return 0;
		}

		const size_t max_to_copy = BAN::Math::min<size_t>(buffer.size(), m_output.bytes);
		size_t to_copy = max_to_copy;
		if (m_termios.c_lflag & ICANON)
			for (to_copy = 1; to_copy < max_to_copy; to_copy++)
				if (m_output.buffer[to_copy - 1] == NL)
					break;

		memcpy(buffer.data(), m_output.buffer.data(), to_copy);

		memmove(m_output.buffer.data(), m_output.buffer.data() + to_copy, m_output.bytes - to_copy);
		m_output.bytes -= to_copy;

		if (m_output.bytes == 0)
			m_output.flush = false;

		m_output.thread_blocker.unblock();

		return to_copy;
	}

	BAN::ErrorOr<size_t> TTY::write_impl(off_t, BAN::ConstByteSpan buffer)
	{
		while (!can_write_impl())
		{
			if (master_has_closed())
				return BAN::Error::from_errno(EIO);
			TRY(Thread::current().block_or_eintr_indefinite(m_write_blocker, &m_mutex));
		}

		size_t written = 0;

		{
			SpinLockGuard _(m_write_lock);
			for (; written < buffer.size(); written++)
				if (!putchar(buffer[written]))
					break;
			after_write();
		}

		if (can_write_impl())
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
