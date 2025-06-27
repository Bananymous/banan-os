#pragma once

#include <LibGUI/Widget/Widget.h>

namespace LibGUI::Widget
{

	class RoundedWidget : public Widget
	{
	public:
		struct Style
		{
			Style(uint32_t color_normal = 0xA0A0A0, uint32_t border_width = 1, uint32_t color_border = 0x000000, uint32_t corner_radius = 5)
				: color_normal(color_normal)
				, border_width(border_width)
				, color_border(color_border)
				, corner_radius(corner_radius)
			{}

			uint32_t color_normal;
			uint32_t border_width;
			uint32_t color_border;
			uint32_t corner_radius;
		};

		Style& style() { return m_style; }
		const Style& style() const { return m_style; }

	protected:
		RoundedWidget(BAN::RefPtr<Widget> parent, Rectangle area)
			: Widget(parent, area)
		{ }

		bool contains(Point point) const override;

		void show_impl() override;

	private:
		Style m_style;
	};

}
