#include <LibGUI/Widget/Widget.h>
#include <LibFont/Font.h>

namespace LibGUI::Widget
{

	static BAN::Optional<LibFont::Font> s_default_font;

	BAN::ErrorOr<void> Widget::set_default_font(BAN::StringView path)
	{
		s_default_font = TRY(LibFont::Font::load(path));
		return {};
	}

	const LibFont::Font& Widget::default_font()
	{
		if (!s_default_font.has_value())
			MUST(set_default_font("/usr/share/fonts/lat0-16.psfu"_sv));
		return s_default_font.value();
	}

	BAN::ErrorOr<BAN::RefPtr<Widget>> Widget::create(BAN::RefPtr<Widget> parent, uint32_t color, Rectangle area)
	{
		auto* widget_ptr = new Widget(parent, area);
		if (widget_ptr == nullptr)
			return BAN::Error::from_errno(ENOMEM);
		auto widget = BAN::RefPtr<Widget>::adopt(widget_ptr);
		TRY(widget->initialize(color));
		return widget;
	}

	BAN::ErrorOr<void> Widget::initialize(uint32_t color)
	{
		m_texture = TRY(Texture::create(width(), height(), color));
		if (m_parent)
			TRY(m_parent->m_children.push_back(this));
		return {};
	}

	bool Widget::is_child_hovered() const
	{
		for (auto child : m_children)
			if (child->m_hovered || child->is_child_hovered())
				return true;
		return false;
	}

	BAN::ErrorOr<void> Widget::set_fixed_geometry(Rectangle area)
	{
		TRY(m_texture.resize(area.w, area.h));
		m_fixed_area = area;
		m_relative_area.clear();

		TRY(update_geometry_impl());

		if (is_shown())
			show();

		return {};
	}

	BAN::ErrorOr<void> Widget::set_relative_geometry(FloatRectangle area)
	{
		if (area.w < 0.0f || area.h < 0.0f)
			return BAN::Error::from_errno(EINVAL);
		ASSERT(m_parent);

		TRY(set_fixed_geometry({
			.x = static_cast<int32_t>(area.x * m_parent->width()),
			.y = static_cast<int32_t>(area.y * m_parent->height()),
			.w = static_cast<uint32_t>(area.w * m_parent->width()),
			.h = static_cast<uint32_t>(area.h * m_parent->height()),
		}));

		m_relative_area = area;
		return {};
	}

	BAN::ErrorOr<void> Widget::update_geometry_impl()
	{
		for (auto child : m_children)
		{
			if (!child->m_relative_area.has_value())
				continue;
			TRY(child->set_relative_geometry(child->m_relative_area.value()));
		}
		return {};
	}

	void Widget::show()
	{
		m_shown = true;

		show_impl();
		m_changed = true;

		for (auto child : m_children)
			if (child->is_shown())
				child->show();
	}

	void Widget::hide()
	{
		if (!is_shown())
			return;
		m_shown = false;

		auto root = m_parent;
		while (root && root->m_parent)
			root = root->m_parent;
		if (root)
			root->show();
	}

	void Widget::before_mouse_move()
	{
		if (!is_shown())
			return;

		m_old_hovered = m_hovered;
		m_hovered = false;

		for (auto child : m_children)
			child->before_mouse_move();
	}

	void Widget::after_mouse_move()
	{
		if (!is_shown())
			return;

		if (m_old_hovered != m_hovered)
			on_hover_change_impl(m_hovered);

		for (auto child : m_children)
			child->after_mouse_move();
	}

	bool Widget::on_mouse_move(LibGUI::EventPacket::MouseMoveEvent::event_t event)
	{
		if (!Rectangle { 0, 0, width(), height() }.contains({ event.x, event.y }))
			return false;
		if (!is_shown())
			return false;

		m_hovered = contains({ event.x, event.y });

		for (auto child : m_children)
		{
			auto rel_event = event;
			rel_event.x -= child->m_fixed_area.x;
			rel_event.y -= child->m_fixed_area.y;
			if (child->on_mouse_move(rel_event))
				return true;
		}

		if (!m_hovered)
			return false;
		return on_mouse_move_impl(event);
	}

	bool Widget::on_mouse_button(LibGUI::EventPacket::MouseButtonEvent::event_t event)
	{
		if (!Rectangle { 0, 0, width(), height() }.contains({ event.x, event.y }))
			return false;
		if (!is_shown())
			return false;

		for (auto child : m_children)
		{
			auto rel_event = event;
			rel_event.x -= child->m_fixed_area.x;
			rel_event.y -= child->m_fixed_area.y;
			if (child->on_mouse_button(rel_event))
				return true;
		}

		if (!contains({ event.x, event.y }))
			return false;
		return on_mouse_button_impl(event);
	}

	Widget::Rectangle Widget::render(Texture& output, Point parent_position, Rectangle out_area)
	{
		if (!is_shown())
			return {};

		update_impl();

		Rectangle invalidated {};
		if (m_changed)
		{
			auto where_i_would_be = Rectangle {
				.x = parent_position.x + m_fixed_area.x,
				.y = parent_position.y + m_fixed_area.y,
				.w = m_fixed_area.w,
				.h = m_fixed_area.h,
			};

			auto where_to_draw = out_area.overlap(where_i_would_be);

			if (where_to_draw.w && where_to_draw.h)
			{
				output.copy_texture(
					m_texture,
					where_to_draw.x,
					where_to_draw.y,
					where_to_draw.x - where_i_would_be.x,
					where_to_draw.y - where_i_would_be.y,
					where_to_draw.w,
					where_to_draw.h
				);
			}

			invalidated = where_to_draw;
			m_changed = false;
		}

		out_area = out_area.overlap({
			.x = parent_position.x + m_fixed_area.x,
			.y = parent_position.y + m_fixed_area.y,
			.w = m_fixed_area.w,
			.h = m_fixed_area.h,
		});

		parent_position = {
			.x = parent_position.x + m_fixed_area.x,
			.y = parent_position.y + m_fixed_area.y,
		};

		for (auto child : m_children)
		{
			invalidated = invalidated.bounding_box(
				child->render(output, parent_position, out_area)
			);
		}

		return invalidated;
	}

}
