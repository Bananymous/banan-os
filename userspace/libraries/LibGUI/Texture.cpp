#include <LibGUI/Texture.h>
#include <LibFont/Font.h>

namespace LibGUI
{

	BAN::ErrorOr<Texture> Texture::create(uint32_t width, uint32_t height, uint32_t color)
	{
		if (BAN::Math::will_addition_overflow(width, height))
			return BAN::Error::from_errno(EOVERFLOW);

		BAN::Vector<uint32_t> pixels;
		TRY(pixels.resize(width * height, color));
		return Texture(BAN::move(pixels), width, height, color);
	}

	BAN::ErrorOr<void> Texture::resize(uint32_t new_width, uint32_t new_height)
	{
		if (BAN::Math::will_addition_overflow(new_width, new_height))
			return BAN::Error::from_errno(EOVERFLOW);

		BAN::Vector<uint32_t> pixels;
		TRY(pixels.resize(new_width * new_height, m_bg_color));

		const uint32_t max_x = BAN::Math::min(new_width,  m_width);
		const uint32_t max_y = BAN::Math::min(new_height, m_height);
		for (uint32_t y = 0; y < max_y; y++)
			for (uint32_t x = 0; x < max_x; x++)
				pixels[y * new_width + x] = m_pixels[y * m_width + x];

		m_width = new_width;
		m_height = new_height;
		m_pixels = BAN::move(pixels);

		return {};
	}

	void Texture::fill_rect(int32_t x, int32_t y, uint32_t width, uint32_t height, uint32_t color)
	{
		if (!clamp_to_texture(x, y, width, height))
			return;
		for (uint32_t y_off = 0; y_off < height; y_off++)
			for (uint32_t x_off = 0; x_off < width; x_off++)
				set_pixel(x + x_off, y + y_off, color);
	}

	void Texture::copy_texture(const Texture& texture, int32_t x, int32_t y)
	{
		uint32_t width  = texture.width();
		uint32_t height = texture.height();
		if (!clamp_to_texture(x, y, width, height))
			return;
		for (uint32_t y_off = 0; y_off < height; y_off++)
			for (uint32_t x_off = 0; x_off < width; x_off++)
				set_pixel(x + x_off, y + y_off, texture.get_pixel(x_off, y_off));
	}

	void Texture::draw_character(uint32_t codepoint, const LibFont::Font& font, int32_t tl_x, int32_t tl_y, uint32_t color)
	{
		if (tl_y + (int32_t)font.height() < 0 || tl_y >= (int32_t)height())
			return;
		if (tl_x + (int32_t)font.width() < 0 || tl_x >= (int32_t)width())
			return;

		auto glyph = font.glyph(codepoint);
		if (glyph == nullptr)
			return;

		for (int32_t off_y = 0; off_y < (int32_t)font.height(); off_y++)
		{
			if (tl_y + off_y < 0)
				continue;
			uint32_t abs_y = tl_y + off_y;
			if (abs_y >= height())
				break;
			for (int32_t off_x = 0; off_x < (int32_t)font.width(); off_x++)
			{
				if (tl_x + off_x < 0)
					continue;
				uint32_t abs_x = tl_x + off_x;
				if (abs_x >= width())
					break;
				const uint8_t bitmask = 1 << (font.width() - off_x - 1);
				if (glyph[off_y * font.pitch()] & bitmask)
					set_pixel(abs_x, abs_y, color);
			}
		}
	}

	void Texture::draw_text(BAN::StringView text, const LibFont::Font& font, int32_t tl_x, int32_t tl_y, uint32_t color)
	{
		for (size_t i = 0; i < text.size(); i++)
			draw_character(text[i], font, tl_x + (int32_t)(i * font.width()), tl_y, color);
	}

	void Texture::shift_vertical(int32_t amount, uint32_t fill_color)
	{
		const uint32_t amount_abs = BAN::Math::abs(amount);
		if (amount_abs == 0 || amount_abs >= height())
			return;

		uint32_t* dst = (amount > 0) ? m_pixels.data() + width() * amount_abs : m_pixels.data();
		uint32_t* src = (amount < 0) ? m_pixels.data() + width() * amount_abs : m_pixels.data();
		memmove(dst, src, width() * (height() - amount_abs) * 4);

		const uint32_t y_lo = (amount < 0) ? height() - amount_abs : 0;
		const uint32_t y_hi = (amount < 0) ? height()              : amount_abs;
		for (uint32_t y = y_lo; y < y_hi; y++)
			for (uint32_t x = 0; x < width(); x++)
				set_pixel(x, y, fill_color);
	}

	void Texture::copy_horizontal_slice(int32_t dst_y, int32_t src_y, uint32_t uamount, uint32_t fill_color)
	{
		int32_t amount = uamount;
		if (dst_y < 0)
		{
			amount -= -dst_y;
			src_y  += -dst_y;
			dst_y   =  0;
		}

		amount = BAN::Math::min<int32_t>(amount, height() - dst_y);
		if (amount <= 0)
			return;

		const int32_t copy_src_y  = BAN::Math::clamp<int32_t>(src_y, 0, height());
		const int32_t copy_amount = BAN::Math::clamp<int32_t>(src_y + amount, 0, height()) - copy_src_y;
		if (copy_amount > 0)
		{
			memmove(
				&m_pixels[width() * (dst_y + (copy_src_y - src_y))],
				&m_pixels[width() * copy_src_y],
				copy_amount * width() * 4
			);
		}

		const uint32_t fill_y_off = (src_y < copy_src_y) ? 0 : copy_amount;
		const uint32_t fill_amount = amount - copy_amount;
		for (uint32_t i = 0; i < fill_amount; i++)
			for (uint32_t x = 0; x < width(); x++)
				set_pixel(x, dst_y + fill_y_off + i, fill_color);
	}

	void Texture::copy_rect(int32_t dst_x, int32_t dst_y, int32_t src_x, int32_t src_y, uint32_t width, uint32_t height, uint32_t fill_color)
	{
		fill_rect(dst_x, dst_y, width, height, fill_color);

		if (!clamp_to_texture(dst_x, dst_y, width, height))
			return;
		if (!clamp_to_texture(src_x, src_y, width, height))
			return;

		const bool copy_dir = dst_y < src_y;
		for (uint32_t i = 0; i < height; i++)
		{
			const uint32_t y_off = copy_dir ? i : height - i - 1;
			memmove(
				&m_pixels[(dst_y + y_off) * this->width()],
				&m_pixels[(src_y + y_off) * this->width()],
				width * 4
			);
		}
	}

	bool Texture::clamp_to_texture(int32_t& signed_x, int32_t& signed_y, uint32_t& width, uint32_t& height) const
	{
		const int32_t min_x = BAN::Math::max<int32_t>(signed_x, 0);
		const int32_t min_y = BAN::Math::max<int32_t>(signed_y, 0);
		const int32_t max_x = BAN::Math::min<int32_t>(this->width(), signed_x + (int32_t)width);
		const int32_t max_y = BAN::Math::min<int32_t>(this->height(), signed_y + (int32_t)height);

		if (min_x >= max_x)
			return false;
		if (min_y >= max_y)
			return false;

		signed_x = min_x;
		signed_y = min_y;
		width = max_x - min_x;
		height = max_y - min_y;
		return true;
	}

}
