#pragma once

#include <BAN/Array.h>
#include <kernel/Device/Device.h>
#include <kernel/Input/KeyEvent.h>
#include <kernel/SpinLock.h>
#include <kernel/Terminal/TerminalDriver.h>
#include <kernel/Terminal/termios.h>
#include <kernel/Semaphore.h>

namespace Kernel
{
	
	class TTY : public CharacterDevice
	{
	public:
		TTY(TerminalDriver*);

		void set_termios(const termios& termios) { m_termios = termios; }
		termios get_termios() const { return m_termios; }	
		void set_font(const Kernel::Font&);

		uint32_t height() const { return m_height; }
		uint32_t width() const { return m_width; }

		void set_foreground_pgrp(pid_t pgrp) { m_foreground_pgrp = pgrp; }
		pid_t foreground_pgrp() const { return m_foreground_pgrp; }

		// for kprint
		static void putchar_current(uint8_t ch);
		static bool is_initialized();
		static BAN::RefPtr<TTY> current();

		void initialize_device();

		virtual BAN::ErrorOr<size_t> read(size_t, void*, size_t) override;
		virtual BAN::ErrorOr<size_t> write(size_t, const void*, size_t) override;
		virtual bool has_data() const override;

	private:
		void clear();
		void putchar(uint8_t ch);
		void reset_ansi();
		void handle_ansi_csi(uint8_t ch);
		void handle_ansi_csi_color();
		void putchar_at(uint32_t codepoint, uint32_t x, uint32_t y);
		void render_from_buffer(uint32_t x, uint32_t y);
		void set_cursor_position(uint32_t x, uint32_t y);

		void on_key(Input::KeyEvent);
		void do_backspace();

	private:
		enum class State
		{
			Normal,
			WaitingAnsiEscape,
			WaitingAnsiCSI,
			WaitingUTF8,
		};

		struct AnsiState
		{
			int32_t nums[2]	{ -1, -1 };
			int32_t index { 0 };
		};

		struct UTF8State
		{
			uint32_t codepoint { 0 };
			uint8_t bytes_missing { 0 };
		};

		struct Cell
		{
			TerminalDriver::Color foreground { TerminalColor::BRIGHT_WHITE };
			TerminalDriver::Color background { TerminalColor::BLACK };
			uint32_t codepoint { ' ' };
		};

	private:
		mutable Kernel::SpinLock m_lock;

		State m_state { State::Normal };
		AnsiState m_ansi_state { };
		UTF8State m_utf8_state { };

		uint32_t m_width { 0 };
		uint32_t m_height { 0 };

		uint32_t m_saved_row { 0 };
		uint32_t m_saved_column { 0 };

		uint32_t m_row { 0 };
		uint32_t m_column { 0 };
		Cell* m_buffer { nullptr };

		TerminalDriver::Color m_foreground { TerminalColor::BRIGHT_WHITE };
		TerminalDriver::Color m_background { TerminalColor::BLACK };

		pid_t m_foreground_pgrp { 0 };

		termios m_termios;

		struct Buffer
		{
			BAN::Array<uint8_t, 1024> buffer;
			size_t bytes { 0 };
			bool flush { false };
			Semaphore semaphore;
		};
		Buffer m_output;

		TerminalDriver* m_terminal_driver { nullptr };

	public:
		virtual dev_t rdev() const override { return m_rdev; }
		virtual bool is_tty() const override { return true; }

	private:
		dev_t m_rdev;
	};

}
