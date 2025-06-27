#include <LibGUI/MessageBox.h>
#include <LibGUI/Window.h>
#include <LibGUI/Widget/Button.h>
#include <LibGUI/Widget/Grid.h>
#include <LibGUI/Widget/TextArea.h>
#include <LibGUI/Widget/Widget.h>

namespace LibGUI
{

	BAN::ErrorOr<void> MessageBox::create(BAN::StringView message, BAN::StringView title)
	{
		BAN::StringView ok_button = "OK";
		TRY(create(message, title, { &ok_button, 1 }));
		return {};
	}

	BAN::ErrorOr<size_t> MessageBox::create(BAN::StringView message, BAN::StringView title, BAN::Span<BAN::StringView> buttons)
	{
		if (buttons.empty())
			return BAN::Error::from_errno(EINVAL);

		const uint32_t window_width = 300;

		auto root_widget = TRY(Widget::Widget::create({}, 0xFFFFFF, { 0, 0, window_width, 0 }));

		auto text_area = TRY(Widget::TextArea::create(root_widget, message, { 0, 0, window_width, 0}));
		text_area->style().border_width = 0;
		text_area->style().color_normal = Widget::Widget::color_invisible;
		text_area->style().corner_radius = 0;
		TRY(text_area->set_relative_geometry({ 0.0, 0.0, 1.0, 0.8 }));
		text_area->show();

		bool waiting = true;
		size_t result = 0;

		auto button_area = TRY(Widget::Grid::create(root_widget, buttons.size(), 1));
		for (size_t i = 0; i < buttons.size(); i++)
		{
			auto button = TRY(Widget::Button::create(button_area, buttons[i]));
			TRY(button_area->set_widget_position(button, i, 1, 0, 1));
			button->set_click_callback([&result, &waiting, i] { result = i; waiting = false; });
			button->show();
		}
		TRY(button_area->set_relative_geometry({ 0.0, 0.8, 1.0, 0.2 }));
		button_area->show();

		const uint32_t button_height = 20;
		const uint32_t window_height = text_area->get_required_height() + button_height;

		auto attributes = Window::default_attributes;
		attributes.resizable = true;

		auto window = TRY(Window::create(window_width, window_height, title, attributes));
		TRY(window->set_root_widget(root_widget));
		window->set_close_window_event_callback([&waiting] { waiting = false; });

		while (waiting)
		{
			window->wait_events();
			window->poll_events();
		}

		return result;
	}

}
