#pragma once

#include <BAN/Function.h>
#include <BAN/StringView.h>

#include <LibGUI/Widget/RoundedWidget.h>

namespace LibGUI::Widget
{

	class Button : public RoundedWidget
	{
	public:
		struct Style : RoundedWidget::Style
		{
			Style()
				: RoundedWidget::Style()
				, color_hovered(0x808080)
				, color_text(0x000000)
			{}

			uint32_t color_hovered;
			uint32_t color_text;
		};

	public:
		static BAN::ErrorOr<BAN::RefPtr<Button>> create(BAN::RefPtr<Widget> parent, BAN::StringView text, Rectangle geometry = {});

		BAN::ErrorOr<void> set_text(BAN::StringView);

		Style& style() { return m_style; }
		const Style& style() const { return m_style; }

		void set_click_callback(BAN::Function<void()> callback) { m_click_callback = callback; }

	protected:
		Button(BAN::RefPtr<Widget> parent, Rectangle area)
			: RoundedWidget(parent, area)
		{ }

		void update_impl() override;
		void show_impl() override;

		bool on_mouse_button_impl(LibGUI::EventPacket::MouseButtonEvent::event_t) override;

	private:
		Style m_style;
		bool m_hover_state { false };
		BAN::String m_text;

		BAN::Function<void()> m_click_callback;
	};

}
