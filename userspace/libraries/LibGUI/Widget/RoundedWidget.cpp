#include <LibGUI/Widget/RoundedWidget.h>

namespace LibGUI::Widget
{

	bool RoundedWidget::contains(Point point) const
	{
		if (!Widget::contains(point))
			return false;

		const auto is_outside_corner =
			[this]<uint8_t quadrant>(Point point) -> bool
			{
				const auto radius = m_style.corner_radius;
				const uint32_t base_x = (quadrant % 2) ? m_texture.width()  - radius - 1 : 0;
				const uint32_t base_y = (quadrant / 2) ? m_texture.height() - radius - 1 : 0;

				if (point.x < static_cast<int32_t>(base_x) || point.x > static_cast<int32_t>(base_x + radius))
					return false;
				if (point.y < static_cast<int32_t>(base_y) || point.y > static_cast<int32_t>(base_y + radius))
					return false;

				const uint32_t x_off = point.x - base_x;
				const uint32_t y_off = point.y - base_y;

				const uint32_t dx = ((quadrant % 2) ? x_off : radius - x_off);
				const uint32_t dy = ((quadrant / 2) ? y_off : radius - y_off);
				const uint32_t distance = dx * dx + dy * dy;

				return distance >= radius * radius;
			};

		if (is_outside_corner.operator()<0>(point))
			return false;
		if (is_outside_corner.operator()<1>(point))
			return false;
		if (is_outside_corner.operator()<2>(point))
			return false;
		if (is_outside_corner.operator()<3>(point))
			return false;
		return true;
	}

	void RoundedWidget::show_impl()
	{
		if (m_style.border_width)
		{
			m_texture.fill_rect(
				0,
				0,
				m_texture.width(),
				m_style.border_width,
				m_style.color_border
			);
			m_texture.fill_rect(
				0,
				0,
				m_style.border_width,
				m_texture.height(),
				m_style.color_border
			);
			m_texture.fill_rect(
				0,
				m_texture.height() - m_style.border_width,
				m_texture.width(),
				m_style.border_width,
				m_style.color_border
			);
			m_texture.fill_rect(
				m_texture.width() - m_style.border_width,
				0,
				m_style.border_width,
				m_texture.height(),
				m_style.color_border
			);
		}

		if (m_style.corner_radius)
		{
			const auto draw_corner =
				[this]<uint8_t quadrant>()
				{
					const auto radius = m_style.corner_radius;
					const uint32_t base_x = (quadrant % 2) ? m_texture.width()  - radius - 1 : 0;
					const uint32_t base_y = (quadrant / 2) ? m_texture.height() - radius - 1 : 0;
					const uint32_t distance_max = radius * radius;
					const uint32_t distance_min = (radius - m_style.border_width) * (radius - m_style.border_width);

					for (uint32_t y_off = 0; y_off <= radius; y_off++)
					{
						for (uint32_t x_off = 0; x_off <= radius; x_off++)
						{
							const uint32_t dx = ((quadrant % 2) ? x_off : radius - x_off);
							const uint32_t dy = ((quadrant / 2) ? y_off : radius - y_off);
							const uint32_t distance = dx * dx + dy * dy;

							if (base_x + x_off >= m_texture.width())
								continue;
							if (base_y + y_off >= m_texture.height())
								continue;

							if (distance > distance_max)
								m_texture.set_pixel(base_x + x_off, base_y + y_off, color_invisible);
							else if (distance >= distance_min)
								m_texture.set_pixel(base_x + x_off, base_y + y_off, m_style.color_border);
						}
					}
				};

			draw_corner.operator()<0>();
			draw_corner.operator()<1>();
			draw_corner.operator()<2>();
			draw_corner.operator()<3>();
		}
	}

}
