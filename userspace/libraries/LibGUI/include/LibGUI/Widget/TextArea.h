#pragma once

#include <BAN/StringView.h>

#include <LibGUI/Widget/RoundedWidget.h>

namespace LibGUI::Widget
{

	class TextArea : public RoundedWidget
	{
	public:
		struct Style : RoundedWidget::Style
		{
			Style()
				: RoundedWidget::Style()
				, color_text(0x000000)
			{}

			uint32_t color_text;
		};

	public:
		static BAN::ErrorOr<BAN::RefPtr<TextArea>> create(BAN::RefPtr<Widget> parent, BAN::StringView text, Rectangle geometry = {});

		BAN::StringView text() const { return m_text; }
		BAN::ErrorOr<void> set_text(BAN::StringView);

		uint32_t get_required_height() const;

		Style& style() { return m_style; }
		const Style& style() const { return m_style; }

	protected:
		TextArea(BAN::RefPtr<Widget> parent, Rectangle area)
			: RoundedWidget(parent, area)
		{ }

		BAN::ErrorOr<void> wrap_text();

		BAN::ErrorOr<void> update_geometry_impl() override;
		void show_impl() override;

	private:
		Style m_style;
		BAN::String m_text;
		BAN::Vector<BAN::String> m_wrapped_text;
	};

}
