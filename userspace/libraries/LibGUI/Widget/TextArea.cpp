#include <LibFont/Font.h>
#include <LibGUI/Widget/TextArea.h>
#include <LibGUI/Window.h>

#include <ctype.h>

namespace LibGUI::Widget
{

	BAN::ErrorOr<BAN::RefPtr<TextArea>> TextArea::create(BAN::RefPtr<Widget> parent, BAN::StringView text, Rectangle geometry)
	{
		auto* text_area_ptr = new TextArea(parent, geometry);
		if (text_area_ptr == nullptr)
			return BAN::Error::from_errno(ENOMEM);
		auto text_area = BAN::RefPtr<TextArea>::adopt(text_area_ptr);
		TRY(text_area->initialize(color_invisible));
		TRY(text_area->set_text(text));
		return text_area;
	}

	BAN::ErrorOr<void> TextArea::set_text(BAN::StringView text)
	{
		m_text.clear();
		TRY(m_text.append(text));
		TRY(wrap_text());
		if (is_shown())
			show();
		return {};
	}

	uint32_t TextArea::get_required_height() const
	{
		auto& font = default_font();
		return m_wrapped_text.size() * font.height();
	}

	BAN::ErrorOr<void> TextArea::wrap_text()
	{
		m_wrapped_text.clear();

		if (width() == 0)
			return {};

		auto& font = default_font();

		const uint32_t total_columns = width() / font.width();
		ASSERT(total_columns != 0);

		TRY(m_wrapped_text.emplace_back());

		for (size_t i = 0; i < m_text.size(); i++)
		{
			if (m_text[i] == '\n')
				TRY(m_wrapped_text.emplace_back());
			else if (isspace(m_text[i]) && m_wrapped_text.back().size() == 0)
				;
			else
			{
				TRY(m_wrapped_text.back().push_back(m_text[i]));

				if (i + 1 < m_text.size() && isspace(m_text[i]) && !isspace(m_text[i + 1]))
				{
					size_t word_len = 0;
					for (size_t j = i + 1; j < m_text.size() && !isspace(m_text[j]); j++)
						word_len++;
					if (word_len <= total_columns && m_wrapped_text.back().size() + word_len > total_columns)
						TRY(m_wrapped_text.emplace_back());
				}

				if (m_wrapped_text.back().size() >= total_columns)
					TRY(m_wrapped_text.emplace_back());
			}
		}

		return {};
	}

	BAN::ErrorOr<void> TextArea::update_geometry_impl()
	{
		TRY(wrap_text());
		return Widget::update_geometry_impl();
	}

	void TextArea::show_impl()
	{
		const auto& font = default_font();

		m_texture.fill(m_style.color_normal);
		for (int32_t row = 0; row < static_cast<int32_t>(m_wrapped_text.size()); row++)
			m_texture.draw_text(m_wrapped_text[row].sv(), font, 0, row * font.height(), m_style.color_text);

		RoundedWidget::style() = m_style;
		RoundedWidget::show_impl();
	}

}
