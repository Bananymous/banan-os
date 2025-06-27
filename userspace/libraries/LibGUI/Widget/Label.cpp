#include <LibFont/Font.h>
#include <LibGUI/Widget/Label.h>
#include <LibGUI/Window.h>

namespace LibGUI::Widget
{

	BAN::ErrorOr<BAN::RefPtr<Label>> Label::create(BAN::RefPtr<Widget> parent, BAN::StringView text, Rectangle geometry)
	{
		auto* label_ptr = new Label(parent, geometry);
		if (label_ptr == nullptr)
			return BAN::Error::from_errno(ENOMEM);
		auto label = BAN::RefPtr<Label>::adopt(label_ptr);
		TRY(label->initialize(color_invisible));
		TRY(label->m_text.append(text));
		return label;
	}

	BAN::ErrorOr<void> Label::set_text(BAN::StringView text)
	{
		m_text.clear();
		TRY(m_text.append(text));
		if (is_shown())
			show();
		return {};
	}

	void Label::show_impl()
	{
		const auto& font = default_font();
		const int32_t text_h = font.height();
		const int32_t text_w = font.width() * m_text.size();

		const int32_t off_x = (static_cast<int32_t>(width())  - text_w) / 2;
		const int32_t off_y = (static_cast<int32_t>(height()) - text_h) / 2;

		m_texture.fill(m_style.color_normal);
		m_texture.draw_text(m_text, font, off_x, off_y, m_style.color_text);

		RoundedWidget::style() = m_style;
		RoundedWidget::show_impl();
	}

}
