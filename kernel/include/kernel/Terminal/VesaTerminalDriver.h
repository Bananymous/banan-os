#pragma once

#include <kernel/Terminal/TerminalDriver.h>

class VesaTerminalDriver final : public TerminalDriver
{
public:
	static VesaTerminalDriver* create();
	~VesaTerminalDriver();

	virtual uint32_t width() const override { return m_width / font().width(); }
	virtual uint32_t height() const override { return m_height / font().height(); }

	virtual void putchar_at(uint16_t, uint32_t, uint32_t, Color, Color) override;
	virtual void clear(Color) override;

	virtual void set_cursor_position(uint32_t, uint32_t) override;

private:
	VesaTerminalDriver(uint32_t width, uint32_t height, uint32_t pitch, uint8_t bpp, uintptr_t address)
		: m_width(width)
		, m_height(height)
		, m_pitch(pitch)
		, m_bpp(bpp)
		, m_address(address)
	{ }

	void set_pixel(uint32_t, Color);

private:
	uint32_t m_width = 0;
	uint32_t m_height = 0;
	uint32_t m_pitch = 0;
	uint8_t m_bpp = 0;
	uintptr_t m_address = 0;

	static constexpr Color s_cursor_color = TerminalColor::BRIGHT_WHITE;
};