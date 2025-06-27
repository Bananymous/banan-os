#include <LibGUI/Widget/Grid.h>

namespace LibGUI::Widget
{

	BAN::ErrorOr<BAN::RefPtr<Grid>> Grid::create(BAN::RefPtr<Widget> parent, uint32_t cols, uint32_t rows, uint32_t color, Rectangle geometry)
	{
		auto* grid_ptr = new Grid(parent, geometry, cols, rows);
		if (grid_ptr == nullptr)
			return BAN::Error::from_errno(ENOMEM);
		auto grid = BAN::RefPtr<Grid>::adopt(grid_ptr);
		TRY(grid->initialize(color));
		return grid;
	}

	Widget::Rectangle Grid::grid_element_area(const GridElement& element) const
	{
		const uint32_t this_x = element.col * width()  / m_cols;
		const uint32_t this_y = element.row * height() / m_rows;

		const uint32_t next_x = (element.col + element.col_span) * width() / m_cols;
		const uint32_t next_y = (element.row + element.row_span) * height() / m_rows;

		return Widget::Rectangle {
			.x = static_cast<int32_t>(this_x),
			.y = static_cast<int32_t>(this_y),
			.w = next_x - this_x,
			.h = next_y - this_y,
		};
	}

	BAN::ErrorOr<void> Grid::update_geometry_impl()
	{
		for (auto& grid_element : m_grid_elements)
			TRY(grid_element.widget->set_fixed_geometry(grid_element_area(grid_element)));
		return {};
	}

	BAN::ErrorOr<void> Grid::set_widget_position(BAN::RefPtr<Widget> widget, uint32_t col, uint32_t col_span, uint32_t row, uint32_t row_span)
	{
		if (col_span == 0 || row_span == 0)
			return BAN::Error::from_errno(EINVAL);
		if (col + col_span > m_cols)
			return BAN::Error::from_errno(EINVAL);
		if (row + row_span > m_rows)
			return BAN::Error::from_errno(EINVAL);
		ASSERT(widget->parent() == this);
		TRY(m_grid_elements.emplace_back(widget, col, col_span, row, row_span));
		TRY(widget->set_fixed_geometry(grid_element_area(m_grid_elements.back())));
		return {};
	}

}
