#pragma once

#include <BAN/StringView.h>

#include <stdint.h>

namespace LibFont { class Font; }

namespace LibGUI
{

	class Texture
	{
	public:
		static constexpr uint32_t color_invisible = 0x69000000;

	public:
		static BAN::ErrorOr<Texture> create(uint32_t width, uint32_t height, uint32_t color);
		Texture() = default;

		BAN::ErrorOr<void> resize(uint32_t width, uint32_t height);

		void set_pixel(uint32_t x, uint32_t y, uint32_t color)
		{
			ASSERT(x < m_width);
			ASSERT(y < m_height);
			if (x < m_clip_x || x >= m_clip_x + m_clip_w)
				return;
			if (y < m_clip_y || y >= m_clip_y + m_clip_h)
				return;
			m_pixels[y * m_width + x] = color;
		}

		uint32_t get_pixel(uint32_t x, uint32_t y) const
		{
			ASSERT(x < m_width);
			ASSERT(y < m_height);
			return m_pixels[y * m_width + x];
		}

		BAN::Span<uint32_t> pixels() { return m_pixels.span(); }

		void set_clip_area(int32_t x, int32_t y, uint32_t width, uint32_t height);

		void fill_rect(int32_t x, int32_t y, uint32_t width, uint32_t height, uint32_t color);
		void fill(uint32_t color) { return fill_rect(0, 0, width(), height(), color); }

		void clear_rect(int32_t x, int32_t y, uint32_t width, uint32_t height) { fill_rect(x, y, width, height, m_bg_color); }
		void clear() { return clear_rect(0, 0, width(), height()); }

		void copy_texture(const Texture& texture, int32_t x, int32_t y, uint32_t sub_x = 0, uint32_t sub_y = 0, uint32_t width = -1, uint32_t height = -1);

		void draw_character(uint32_t codepoint, const LibFont::Font& font, int32_t x, int32_t y, uint32_t color);
		void draw_text(BAN::StringView text, const LibFont::Font& font, int32_t x, int32_t y, uint32_t color);

		// shift whole vertically by amount pixels, sign determines the direction
		void shift_vertical(int32_t amount);

		// copy horizontal slice [src_y, src_y + amount[ to [dst_y, dst_y + amount[
		void copy_horizontal_slice(int32_t dst_y, int32_t src_y, uint32_t amount);

		// copy rect (src_x, src_y, width, height) to (dst_x, dst_y, width, height)
		void copy_rect(int32_t dst_x, int32_t dst_y, int32_t src_x, int32_t src_y, uint32_t width, uint32_t height);

		uint32_t width() const { return m_width; }
		uint32_t height() const { return m_height; }

		// used on resize to fill empty space
		void set_bg_color(uint32_t bg_color) { m_bg_color = bg_color; }

	private:
		Texture(BAN::Vector<uint32_t>&& pixels, uint32_t width, uint32_t height, uint32_t color)
			: m_pixels(BAN::move(pixels))
			, m_width(width)
			, m_height(height)
			, m_bg_color(color)
			, m_clip_x(0)
			, m_clip_y(0)
			, m_clip_w(width)
			, m_clip_h(height)
		{}

		bool clamp_to_texture(int32_t& x, int32_t& y, uint32_t& width, uint32_t& height) const;
		bool clamp_to_texture(int32_t& dst_x, int32_t& dst_y, int32_t& src_x, int32_t& src_y, uint32_t& width, uint32_t& height, const Texture&) const;

	private:
		BAN::Vector<uint32_t> m_pixels;
		uint32_t m_width { 0 };
		uint32_t m_height { 0 };
		uint32_t m_bg_color { 0xFFFFFFFF };

		uint32_t m_clip_x { 0 };
		uint32_t m_clip_y { 0 };
		uint32_t m_clip_w { 0 };
		uint32_t m_clip_h { 0 };
		bool m_has_set_clip { false };

		friend class Window;
	};

}
