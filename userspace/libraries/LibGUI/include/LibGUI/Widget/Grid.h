#pragma once

#include <LibGUI/Widget/Widget.h>

namespace LibGUI::Widget
{

	class Grid : public Widget
	{
	public:
		static BAN::ErrorOr<BAN::RefPtr<Grid>> create(BAN::RefPtr<Widget> parent, uint32_t cols, uint32_t rows, uint32_t color = color_invisible, Rectangle geometry = {});

		BAN::ErrorOr<void> set_widget_position(BAN::RefPtr<Widget> widget, uint32_t col, uint32_t col_span, uint32_t row, uint32_t row_span);

	protected:
		Grid(BAN::RefPtr<Widget> parent, Rectangle geometry, uint32_t cols, uint32_t rows)
			: Widget(parent, geometry)
			, m_cols(cols)
			, m_rows(rows)
		{ }

		BAN::ErrorOr<void> update_geometry_impl() override;

	private:
		struct GridElement
		{
			BAN::RefPtr<Widget> widget;
			uint32_t col;
			uint32_t col_span;
			uint32_t row;
			uint32_t row_span;
		};

		Rectangle grid_element_area(const GridElement& element) const;

	private:
		const uint32_t m_cols;
		const uint32_t m_rows;
		BAN::Vector<GridElement> m_grid_elements;
	};

}
