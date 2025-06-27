#pragma once

#include <BAN/RefPtr.h>

#include <LibGUI/Texture.h>
#include <LibGUI/Packet.h>

namespace LibGUI { class Window; }

namespace LibGUI::Widget
{

	class Widget : public BAN::RefCounted<Widget>
	{
	public:
		static constexpr uint32_t color_invisible = Texture::color_invisible;

		struct Point
		{
			int32_t x, y;
		};

		struct FloatRectangle
		{
			float x, y;
			float w, h;
		};

		struct Rectangle
		{
			int32_t x, y;
			uint32_t w, h;

			struct Bounds
			{
				int32_t min_x, min_y;
				int32_t max_x, max_y;
			};

			bool contains(Point point) const
			{
				if (point.x < x || point.x >= x + static_cast<int32_t>(w))
					return false;
				if (point.y < y || point.y >= y + static_cast<int32_t>(h))
					return false;
				return true;
			}

			Bounds bounds(Rectangle other) const
			{
				return Bounds {
					.min_x = BAN::Math::max(x, other.x),
					.min_y = BAN::Math::max(y, other.y),
					.max_x = BAN::Math::min(x + static_cast<int32_t>(w), other.x + static_cast<int32_t>(other.w)),
					.max_y = BAN::Math::min(y + static_cast<int32_t>(h), other.y + static_cast<int32_t>(other.h)),
				};
			};

			Rectangle overlap(Rectangle other) const
			{
				const auto min_x = BAN::Math::max(x, other.x);
				const auto min_y = BAN::Math::max(y, other.y);
				const auto max_x = BAN::Math::min(x + static_cast<int32_t>(w), other.x + static_cast<int32_t>(other.w));
				const auto max_y = BAN::Math::min(y + static_cast<int32_t>(h), other.y + static_cast<int32_t>(other.h));
				if (min_x >= max_x || min_y >= max_y)
					return {};
				return Rectangle {
					.x = min_x,
					.y = min_y,
					.w = static_cast<uint32_t>(max_x - min_x),
					.h = static_cast<uint32_t>(max_y - min_y),
				};
			}

			Rectangle bounding_box(Rectangle other) const
			{
				if (w == 0 || h == 0)
					return other;
				if (other.w == 0 || other.h == 0)
					return *this;
				const auto min_x = BAN::Math::min(x, other.x);
				const auto min_y = BAN::Math::min(y, other.y);
				const auto max_x = BAN::Math::max(x + static_cast<int32_t>(w), other.x + static_cast<int32_t>(other.w));
				const auto max_y = BAN::Math::max(y + static_cast<int32_t>(h), other.y + static_cast<int32_t>(other.h));
				return Rectangle {
					.x = min_x,
					.y = min_y,
					.w = static_cast<uint32_t>(max_x - min_x),
					.h = static_cast<uint32_t>(max_y - min_y),
				};
			}
		};

	public:
		static BAN::ErrorOr<BAN::RefPtr<Widget>> create(BAN::RefPtr<Widget> parent, uint32_t color = color_invisible, Rectangle geometry = {});

		static BAN::ErrorOr<void> set_default_font(BAN::StringView path);
		static const LibFont::Font& default_font();

		void show();
		void hide();

		BAN::ErrorOr<void> set_fixed_geometry(Rectangle);
		BAN::ErrorOr<void> set_relative_geometry(FloatRectangle);

		BAN::RefPtr<Widget> parent() { return m_parent; }

		uint32_t width() const { return m_fixed_area.w; }
		uint32_t height() const { return m_fixed_area.h; }

	private:
		void before_mouse_move();
		void after_mouse_move();
		bool on_mouse_move(LibGUI::EventPacket::MouseMoveEvent::event_t);

		bool on_mouse_button(LibGUI::EventPacket::MouseButtonEvent::event_t);

	protected:
		Widget(BAN::RefPtr<Widget> parent, Rectangle area)
			: m_parent(parent)
			, m_fixed_area(area)
		{ }

		BAN::ErrorOr<void> initialize(uint32_t color);

		virtual bool contains(Point point) const { return Rectangle { 0, 0, width(), height() }.contains(point); }

		bool is_hovered() const { return m_hovered; }
		bool is_child_hovered() const;

		bool is_shown() const { return m_shown; }

		Rectangle render(Texture& output, Point parent_position, Rectangle out_area);

		virtual void update_impl() {}
		virtual void show_impl() {}

		virtual BAN::ErrorOr<void> update_geometry_impl();

		virtual void on_hover_change_impl(bool hovered) { (void)hovered; }
		virtual bool on_mouse_move_impl(LibGUI::EventPacket::MouseMoveEvent::event_t) { return true; }
		virtual bool on_mouse_button_impl(LibGUI::EventPacket::MouseButtonEvent::event_t) { return true; }

	protected:
		Texture m_texture;

	private:
		BAN::RefPtr<Widget> m_parent;
		BAN::Vector<BAN::RefPtr<Widget>> m_children;
		bool m_shown { false };

		Rectangle m_fixed_area;
		BAN::Optional<FloatRectangle> m_relative_area;

		bool m_changed { false };

		bool m_hovered { false };
		bool m_old_hovered { false };

		friend class LibGUI::Window;
	};

}
