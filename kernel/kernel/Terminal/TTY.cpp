#include <BAN/Errors.h>
#include <BAN/ScopeGuard.h>
#include <BAN/UTF8.h>
#include <kernel/Debug.h>
#include <kernel/FS/DevFS/FileSystem.h>
#include <kernel/LockGuard.h>
#include <kernel/Process.h>
#include <kernel/Terminal/TTY.h>

#include <fcntl.h>
#include <string.h>
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

		auto inode_or_error = DevFileSystem::get().root_inode()->find_inode("tty");
		if (inode_or_error.is_error())
		{
			if (inode_or_error.error().get_error_code() == ENOENT)
				DevFileSystem::get().add_device("tty"sv, MUST(RamSymlinkInode::create(DevFileSystem::get(), s_tty->name(), S_IFLNK | 0666, 0, 0)));
			else
				dwarnln("{}", inode_or_error.error());
			return;
		}

		auto inode = inode_or_error.release_value();
		if (inode->mode().iflnk())
			MUST(((RamSymlinkInode*)inode.ptr())->set_link_target(name()));
	}

	void TTY::initialize_devices()
	{
		static bool initialized = false;
		ASSERT(!initialized);

		Process::create_kernel(
			[](void*)
			{
				int fd = MUST(Process::current().sys_open("/dev/input0"sv, O_RDONLY));
				while (true)
				{
					Input::KeyEvent event;
					ASSERT(MUST(Process::current().sys_read(fd, &event, sizeof(event))) == sizeof(event));
					TTY::current()->on_key_event(event);
				}
			}, nullptr
		);

		initialized = true;
	}

	void TTY::on_key_event(Input::KeyEvent event)
	{
		LockGuard _(m_lock);

		if (event.released())
			return;

		const char* ansi_c_str = Input::key_event_to_utf8(event);

		if (event.ctrl())
		{
			ansi_c_str = nullptr;			
			switch (event.key)
			{
				case Input::Key::A: ansi_c_str = "\x01"; break;
				case Input::Key::B: ansi_c_str = "\x02"; break;
				case Input::Key::C: ansi_c_str = "\x03"; break;
				case Input::Key::D: ansi_c_str = "\x04"; break;
				case Input::Key::E: ansi_c_str = "\x05"; break;
				case Input::Key::F: ansi_c_str = "\x06"; break;
				case Input::Key::G: ansi_c_str = "\x07"; break;
				case Input::Key::H: ansi_c_str = "\x08"; break;
				case Input::Key::I: ansi_c_str = "\x09"; break;
				case Input::Key::J: ansi_c_str = "\x0A"; break;
				case Input::Key::K: ansi_c_str = "\x0B"; break;
				case Input::Key::L: ansi_c_str = "\x0C"; break;
				case Input::Key::M: ansi_c_str = "\x0D"; break;
				case Input::Key::N: ansi_c_str = "\x0E"; break;
				case Input::Key::O: ansi_c_str = "\x0F"; break;
				case Input::Key::P: ansi_c_str = "\x10"; break;
				case Input::Key::Q: ansi_c_str = "\x11"; break;
				case Input::Key::R: ansi_c_str = "\x12"; break;
				case Input::Key::S: ansi_c_str = "\x13"; break;
				case Input::Key::T: ansi_c_str = "\x14"; break;
				case Input::Key::U: ansi_c_str = "\x15"; break;
				case Input::Key::V: ansi_c_str = "\x16"; break;
				case Input::Key::W: ansi_c_str = "\x17"; break;
				case Input::Key::X: ansi_c_str = "\x18"; break;
				case Input::Key::Y: ansi_c_str = "\x19"; break;
				case Input::Key::Z: ansi_c_str = "\x1A"; break;
				default: break;
			}
		}
		else
		{
			switch (event.key)
			{
				case Input::Key::Enter:
				case Input::Key::NumpadEnter:
					ansi_c_str = "\n";
					break;
				case Input::Key::Backspace:
					ansi_c_str = "\b";
					break;
				case Input::Key::ArrowUp:
					ansi_c_str = "\e[A";
					break;
				case Input::Key::ArrowDown:
					ansi_c_str = "\e[B";
					break;
				case Input::Key::ArrowRight:
					ansi_c_str = "\e[C";
					break;
				case Input::Key::ArrowLeft:
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

		LockGuard _(m_lock);

		// ^C
		if (ch == '\x03')
		{
			if (auto ret = Process::sys_kill(-m_foreground_pgrp, SIGINT); ret.is_error())
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

	BAN::ErrorOr<size_t> TTY::read_impl(off_t, void* buffer, size_t count)
	{
		LockGuard _(m_lock);
		while (!m_output.flush)
		{
			m_lock.unlock();
			m_output.semaphore.block();
			m_lock.lock();
		}

		if (m_output.bytes == 0)
		{
			m_output.flush = false;
			return 0;
		}

		size_t to_copy = BAN::Math::min<size_t>(count, m_output.bytes);
		memcpy(buffer, m_output.buffer.data(), to_copy);

		memmove(m_output.buffer.data(), m_output.buffer.data() + to_copy, m_output.bytes - to_copy);
		m_output.bytes -= to_copy;

		if (m_output.bytes == 0)
			m_output.flush = false;

		m_output.semaphore.unblock();

		return to_copy;
	}

	BAN::ErrorOr<size_t> TTY::write_impl(off_t, const void* buffer, size_t count)
	{
		LockGuard _(m_lock);
		for (size_t i = 0; i < count; i++)
			putchar(((uint8_t*)buffer)[i]);
		return count;
	}

	bool TTY::has_data() const
	{
		LockGuard _(m_lock);
		return m_output.flush;
	}

	void TTY::putchar_current(uint8_t ch)
	{
		ASSERT(s_tty);
		LockGuard _(s_tty->m_lock);
		s_tty->putchar(ch);
	}

	bool TTY::is_initialized()
	{
		return s_tty;
	}

}
