#pragma once

#include <BAN/StringView.h>

#include <stdint.h>

namespace LibFont { class Font; }

namespace LibGUI
{

	class Texture
	{
	public:
		static BAN::ErrorOr<Texture> create(uint32_t width, uint32_t height, uint32_t color);

		BAN::ErrorOr<void> resize(uint32_t width, uint32_t height);

		void set_pixel(uint32_t x, uint32_t y, uint32_t color)
		{
			ASSERT(x < m_width);
			ASSERT(y < m_height);
			m_pixels[y * m_width + x] = color;
		}

		uint32_t get_pixel(uint32_t x, uint32_t y) const
		{
			ASSERT(x < m_width);
			ASSERT(y < m_height);
			return m_pixels[y * m_width + x];
		}

		BAN::Span<uint32_t> pixels() { return m_pixels.span(); }

		void fill_rect(int32_t x, int32_t y, uint32_t width, uint32_t height, uint32_t color);
		void fill(uint32_t color) { return fill_rect(0, 0, width(), height(), color); }

		void copy_texture(const Texture& texture, int32_t x, int32_t y);

		void draw_character(uint32_t codepoint, const LibFont::Font& font, int32_t x, int32_t y, uint32_t color);
		void draw_text(BAN::StringView text, const LibFont::Font& font, int32_t x, int32_t y, uint32_t color);

		// shift whole vertically by amount pixels, sign determines the direction
		// fill_color is used to fill "new" data
		void shift_vertical(int32_t amount, uint32_t fill_color);

		// copy horizontal slice [src_y, src_y + amount[ to [dst_y, dst_y + amount[
		// fill_color is used when copying data outside of window bounds
		void copy_horizontal_slice(int32_t dst_y, int32_t src_y, uint32_t amount, uint32_t fill_color);

		// copy rect (src_x, src_y, width, height) to (dst_x, dst_y, width, height)
		// fill_color is used when copying data outside of window bounds
		void copy_rect(int32_t dst_x, int32_t dst_y, int32_t src_x, int32_t src_y, uint32_t width, uint32_t height, uint32_t fill_color);

		uint32_t width() const { return m_width; }
		uint32_t height() const { return m_height; }

		// used on resize to fill empty space
		void set_bg_color(uint32_t bg_color) { m_bg_color = bg_color; }

	private:
		Texture() = default;
		Texture(BAN::Vector<uint32_t>&& pixels, uint32_t width, uint32_t height, uint32_t color)
			: m_pixels(BAN::move(pixels))
			, m_width(width)
			, m_height(height)
			, m_bg_color(color)
		{}

		bool clamp_to_texture(int32_t& x, int32_t& y, uint32_t& width, uint32_t& height) const;

	private:
		BAN::Vector<uint32_t> m_pixels;
		uint32_t m_width { 0 };
		uint32_t m_height { 0 };
		uint32_t m_bg_color { 0xFFFFFFFF };

		friend class Window;
	};

}
