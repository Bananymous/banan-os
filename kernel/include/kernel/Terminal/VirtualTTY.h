#pragma once

#include <BAN/Array.h>
#include <kernel/Device/Device.h>
#include <kernel/Terminal/TerminalDriver.h>
#include <kernel/Terminal/termios.h>
#include <kernel/Terminal/TTY.h>
#include <kernel/Semaphore.h>

namespace Kernel
{

	class VirtualTTY : public TTY
	{
	public:
		static BAN::ErrorOr<BAN::RefPtr<VirtualTTY>> create(TerminalDriver*);

		virtual void set_font(const Font&) override;

		virtual uint32_t height() const override { return m_height; }
		virtual uint32_t width() const override { return m_width; }

		virtual void clear() override;

	protected:
		virtual BAN::StringView name() const override { return m_name; }
		virtual void putchar_impl(uint8_t ch) override;

	private:
		VirtualTTY(TerminalDriver*);

		void reset_ansi();
		void handle_ansi_csi(uint8_t ch);
		void handle_ansi_csi_color();
		void putchar_at(uint32_t codepoint, uint32_t x, uint32_t y);
		void render_from_buffer(uint32_t x, uint32_t y);
		void set_cursor_position(uint32_t x, uint32_t y);

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
			bool question { false };
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
		BAN::String m_name;

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
		bool m_show_cursor { true };

		TerminalDriver* m_terminal_driver { nullptr };

	public:
		virtual dev_t rdev() const override { return m_rdev; }
	private:
		const dev_t m_rdev;
	};

}
