#include <BAN/Errors.h>
#include <BAN/ScopeGuard.h>
#include <BAN/UTF8.h>
#include <kernel/Debug.h>
#include <kernel/FS/DevFS/FileSystem.h>
#include <kernel/FS/VirtualFileSystem.h>
#include <kernel/Lock/LockGuard.h>
#include <kernel/Process.h>
#include <kernel/Terminal/TTY.h>
#include <LibInput/KeyboardLayout.h>

#include <fcntl.h>
#include <string.h>
#include <sys/banan-os.h>
#include <sys/sysmacros.h>

namespace Kernel
{

	static BAN::RefPtr<TTY> s_tty;

	BAN::RefPtr<TTY> TTY::current()
	{
		ASSERT(s_tty);
		return s_tty;
	}

	void TTY::set_as_current()
	{
		s_tty = this;
		clear();

		auto inode_or_error = DevFileSystem::get().root_inode()->find_inode("tty"sv);
		if (inode_or_error.is_error())
		{
			if (inode_or_error.error().get_error_code() == ENOENT)
				DevFileSystem::get().add_inode("tty"sv, MUST(TmpSymlinkInode::create_new(DevFileSystem::get(), 0666, 0, 0, s_tty->name())));
			else
				dwarnln("{}", inode_or_error.error());
			return;
		}

		auto inode = inode_or_error.release_value();
		if (inode->mode().iflnk())
			MUST(static_cast<TmpSymlinkInode&>(*inode.ptr()).set_link_target(name()));
	}

	BAN::ErrorOr<void> TTY::tty_ctrl(int command, int flags)
	{
		if (flags & ~(TTY_FLAG_ENABLE_INPUT | TTY_FLAG_ENABLE_OUTPUT))
			return BAN::Error::from_errno(EINVAL);

		switch (command)
		{
			case TTY_CMD_SET:
				if ((flags & TTY_FLAG_ENABLE_INPUT) && !m_tty_ctrl.receive_input)
				{
					m_tty_ctrl.receive_input = true;
					m_tty_ctrl.semaphore.unblock();
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

	void TTY::initialize_devices()
	{
		static bool initialized = false;
		ASSERT(!initialized);

		Process::create_kernel(
			[](void*)
			{
				auto file_or_error = VirtualFileSystem::get().file_from_absolute_path({ 0, 0, 0, 0 }, "/dev/input0"sv, O_RDONLY);
				if (file_or_error.is_error())
				{
					dprintln("no input device found");
					return;
				}

				auto inode = file_or_error.value().inode;
				while (true)
				{
					while (!TTY::current()->m_tty_ctrl.receive_input)
						TTY::current()->m_tty_ctrl.semaphore.block_indefinite();

					LibInput::RawKeyEvent event;
					size_t read = MUST(inode->read(0, BAN::ByteSpan::from(event)));
					ASSERT(read == sizeof(event));

					TTY::current()->on_key_event(LibInput::KeyboardLayout::get().key_event_from_raw(event));
				}
			}, nullptr
		);

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

	void TTY::on_key_event(LibInput::KeyEvent event)
	{
		LockGuard _(m_mutex);

		if (event.released())
			return;

		const char* ansi_c_str = LibInput::key_to_utf8(event.key, event.modifier);

		if (event.ctrl())
		{
			ansi_c_str = nullptr;
			switch (event.key)
			{
				case LibInput::Key::A: ansi_c_str = "\x01"; break;
				case LibInput::Key::B: ansi_c_str = "\x02"; break;
				case LibInput::Key::C: ansi_c_str = "\x03"; break;
				case LibInput::Key::D: ansi_c_str = "\x04"; break;
				case LibInput::Key::E: ansi_c_str = "\x05"; break;
				case LibInput::Key::F: ansi_c_str = "\x06"; break;
				case LibInput::Key::G: ansi_c_str = "\x07"; break;
				case LibInput::Key::H: ansi_c_str = "\x08"; break;
				case LibInput::Key::I: ansi_c_str = "\x09"; break;
				case LibInput::Key::J: ansi_c_str = "\x0A"; break;
				case LibInput::Key::K: ansi_c_str = "\x0B"; break;
				case LibInput::Key::L: ansi_c_str = "\x0C"; break;
				case LibInput::Key::M: ansi_c_str = "\x0D"; break;
				case LibInput::Key::N: ansi_c_str = "\x0E"; break;
				case LibInput::Key::O: ansi_c_str = "\x0F"; break;
				case LibInput::Key::P: ansi_c_str = "\x10"; break;
				case LibInput::Key::Q: ansi_c_str = "\x11"; break;
				case LibInput::Key::R: ansi_c_str = "\x12"; break;
				case LibInput::Key::S: ansi_c_str = "\x13"; break;
				case LibInput::Key::T: ansi_c_str = "\x14"; break;
				case LibInput::Key::U: ansi_c_str = "\x15"; break;
				case LibInput::Key::V: ansi_c_str = "\x16"; break;
				case LibInput::Key::W: ansi_c_str = "\x17"; break;
				case LibInput::Key::X: ansi_c_str = "\x18"; break;
				case LibInput::Key::Y: ansi_c_str = "\x19"; break;
				case LibInput::Key::Z: ansi_c_str = "\x1A"; break;
				default: break;
			}
		}
		else
		{
			switch (event.key)
			{
				case LibInput::Key::Enter:
				case LibInput::Key::NumpadEnter:
					ansi_c_str = "\n";
					break;
				case LibInput::Key::Backspace:
					ansi_c_str = "\b";
					break;
				case LibInput::Key::ArrowUp:
					ansi_c_str = "\e[A";
					break;
				case LibInput::Key::ArrowDown:
					ansi_c_str = "\e[B";
					break;
				case LibInput::Key::ArrowRight:
					ansi_c_str = "\e[C";
					break;
				case LibInput::Key::ArrowLeft:
					ansi_c_str = "\e[D";
					break;
				default:
					break;
			}
		}

		if (ansi_c_str)
		{
			auto* ptr = (const uint8_t*)ansi_c_str;
			while (*ptr)
				handle_input_byte(*ptr++);
		}
	}

	void TTY::handle_input_byte(uint8_t ch)
	{
		if (ch == 0)
			return;

		LockGuard _(m_mutex);

		// ^C
		if (ch == '\x03')
		{
			if (auto ret = Process::current().sys_kill(-m_foreground_pgrp, SIGINT); ret.is_error())
				dwarnln("TTY: {}", ret.error());
			return;
		}

		// ^D + canonical
		if (ch == '\x04' && m_termios.canonical)
		{
			m_output.flush = true;
			m_output.semaphore.unblock();
			return;
		}

		// backspace + canonical
		if (ch == '\b' && m_termios.canonical)
		{
			do_backspace();
			return;
		}

		m_output.buffer[m_output.bytes++] = ch;

		if (m_termios.echo)
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

		if (ch == '\n' || !m_termios.canonical)
		{
			m_output.flush = true;
			m_output.semaphore.unblock();
		}
	}

	void TTY::do_backspace()
	{
		auto print_backspace =
			[this]
			{
				putchar('\b');
				putchar(' ');
				putchar('\b');
			};

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
				print_backspace();
			}
			// Caret notation
			else if (last < 32 || last == 127)
			{
				m_output.bytes--;
				print_backspace();
				print_backspace();
			}
			// Ascii
			else
			{
				m_output.bytes--;
				print_backspace();
			}
		}
	}

	void TTY::putchar(uint8_t ch)
	{
		SpinLockGuard _(m_write_lock);
		if (m_tty_ctrl.draw_graphics)
			putchar_impl(ch);
	}

	BAN::ErrorOr<size_t> TTY::read_impl(off_t, BAN::ByteSpan buffer)
	{
		while (!m_output.flush)
		{
			LockFreeGuard _(m_mutex);
			TRY(Thread::current().block_or_eintr_indefinite(m_output.semaphore));
		}

		if (m_output.bytes == 0)
		{
			m_output.flush = false;
			return 0;
		}

		size_t to_copy = BAN::Math::min<size_t>(buffer.size(), m_output.bytes);
		memcpy(buffer.data(), m_output.buffer.data(), to_copy);

		memmove(m_output.buffer.data(), m_output.buffer.data() + to_copy, m_output.bytes - to_copy);
		m_output.bytes -= to_copy;

		if (m_output.bytes == 0)
			m_output.flush = false;

		m_output.semaphore.unblock();

		return to_copy;
	}

	BAN::ErrorOr<size_t> TTY::write_impl(off_t, BAN::ConstByteSpan buffer)
	{
		SpinLockGuard _(m_write_lock);
		for (size_t i = 0; i < buffer.size(); i++)
			putchar(buffer[i]);
		return buffer.size();
	}

	void TTY::putchar_current(uint8_t ch)
	{
		ASSERT(s_tty);
		SpinLockGuard _(s_tty->m_write_lock);
		s_tty->putchar(ch);
	}

	bool TTY::is_initialized()
	{
		return !!s_tty;
	}

}
