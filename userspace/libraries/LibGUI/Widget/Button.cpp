#include <LibFont/Font.h>
#include <LibGUI/Widget/Button.h>
#include <LibGUI/Window.h>

namespace LibGUI::Widget
{

	BAN::ErrorOr<BAN::RefPtr<Button>> Button::create(BAN::RefPtr<Widget> parent, BAN::StringView text, Rectangle geometry)
	{
		auto* button_ptr = new Button(parent, geometry);
		if (button_ptr == nullptr)
			return BAN::Error::from_errno(ENOMEM);
		auto button = BAN::RefPtr<Button>::adopt(button_ptr);
		TRY(button->initialize(color_invisible));
		TRY(button->m_text.append(text));
		return button;
	}

	BAN::ErrorOr<void> Button::set_text(BAN::StringView text)
	{
		m_text.clear();
		TRY(m_text.append(text));
		if (is_shown())
			show();
		return {};
	}

	void Button::update_impl()
	{
		const bool hover_color = is_hovered() && !is_child_hovered();
		if (hover_color != m_hover_state)
			show();
	}

	void Button::show_impl()
	{
		m_hover_state = is_hovered() && !is_child_hovered();

		const auto& font = default_font();
		const int32_t text_h = font.height();
		const int32_t text_w = font.width() * m_text.size();

		const int32_t off_x = (static_cast<int32_t>(width())  - text_w) / 2;
		const int32_t off_y = (static_cast<int32_t>(height()) - text_h) / 2;

		m_texture.fill(m_hover_state ? m_style.color_hovered : m_style.color_normal);
		m_texture.draw_text(m_text, font, off_x, off_y, m_style.color_text);

		RoundedWidget::style() = m_style;
		RoundedWidget::show_impl();
	}

	bool Button::on_mouse_button_impl(LibGUI::EventPacket::MouseButtonEvent::event_t event)
	{
		if (event.pressed && event.button == LibInput::MouseButton::Left && m_click_callback)
			m_click_callback();
		return true;
	}

}
