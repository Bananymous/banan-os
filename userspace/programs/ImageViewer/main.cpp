#include <BAN/Math.h>
#include <LibGUI/Window.h>
#include <LibImage/Image.h>

#include <stdio.h>

static constexpr size_t s_window_width = 400;
static constexpr size_t s_window_height = 400;
static constexpr float s_scale_factor = 0.9f;

static void run(LibImage::Image& image, LibGUI::Window& window)
{
	int32_t xoff = static_cast<int32_t>(window.width()  - image.width())  / 2;
	int32_t yoff = static_cast<int32_t>(window.height() - image.height()) / 2;
	float scale = 1.0f;

	bool mouse_pressed = false;
	int32_t mouse_x = 0;
	int32_t mouse_y = 0;

	bool should_close = false;
	bool should_display = false;
	bool linear = false;

	const auto display_image =
		[&]() -> void
		{
			window.texture().fill(0xFFFFFF);

			for (int32_t winy = 0; winy < static_cast<int32_t>(window.height()); winy++)
			{
				for (int32_t winx = 0; winx < static_cast<int32_t>(window.width()); winx++)
				{
					LibImage::Image::Color color;

					if (!linear)
					{
						const int32_t imgx = BAN::Math::round((winx - xoff) / scale);
						const int32_t imgy = BAN::Math::round((winy - yoff) / scale);
						if (imgx < 0 || imgx >= static_cast<int32_t>(image.width()))
							continue;
						if (imgy < 0 || imgy >= static_cast<int32_t>(image.height()))
							continue;
						color = image.get_color(imgx, imgy);
					}
					else
					{
						const float fimgx = (winx - xoff) / scale;
						const float fimgy = (winy - yoff) / scale;
						if (fimgx + 0.5f < 0 || fimgx + 0.5f >= image.width())
							continue;
						if (fimgy + 0.5f < 0 || fimgy + 0.5f >= image.height())
							continue;

						const int32_t imgl = BAN::Math::clamp<int32_t>(fimgx,    0, image.width()  - 1);
						const int32_t imgr = BAN::Math::clamp<int32_t>(imgl + 1, 0, image.width()  - 1);
						const int32_t imgt = BAN::Math::clamp<int32_t>(fimgy,    0, image.height() - 1);
						const int32_t imgb = BAN::Math::clamp<int32_t>(imgt + 1, 0, image.height() - 1);

						const auto tl = image.get_color(imgl, imgt);
						const auto tr = image.get_color(imgr, imgt);
						const auto bl = image.get_color(imgl, imgb);
						const auto br = image.get_color(imgr, imgb);

						const float weightx = fimgx - BAN::Math::floor(fimgx);
						const float weighty = fimgy - BAN::Math::floor(fimgy);

						color = LibImage::Image::Color::average(
							LibImage::Image::Color::average(tl, tr, weightx),
							LibImage::Image::Color::average(bl, br, weightx),
							weighty
						);
					}

					window.texture().set_pixel(winx, winy, color.as_argb());
				}
			}

			window.invalidate();
		};

	window.set_close_window_event_callback([&] {
		should_close = true;
	});

	window.set_resize_window_event_callback([&] {
		should_display = true;
	});

	window.set_mouse_button_event_callback([&](auto event) {
		if (event.button != LibInput::MouseButton::Left)
			return;
		mouse_pressed = event.pressed;
		mouse_x = event.x;
		mouse_y = event.y;
	});

	window.set_mouse_move_event_callback([&](auto event) {
		if (mouse_pressed)
		{
			xoff += event.x - mouse_x;
			yoff += event.y - mouse_y;
			should_display = true;
		}
		mouse_x = event.x;
		mouse_y = event.y;
	});

	window.set_mouse_scroll_event_callback([&](auto event) {
		const float new_scale = scale * BAN::Math::pow<float>(s_scale_factor, -event.scroll);
		xoff = mouse_x - (mouse_x - xoff) / scale * new_scale;
		yoff = mouse_y - (mouse_y - yoff) / scale * new_scale;
		scale = new_scale;
		should_display = true;
	});

	window.set_key_event_callback([&](LibGUI::EventPacket::KeyEvent::event_t event) {
		if (!event.pressed())
			return;
		switch (event.key)
		{
			case LibInput::Key::Space:
				linear = !linear;
				should_display = true;
				break;
			default:
				break;
		}
	});

	display_image();

	auto attributes = window.get_attributes();
	attributes.shown = true;
	window.set_attributes(attributes);

	while (!should_close)
	{
		window.wait_events();
		window.poll_events();

		if (should_display)
			display_image();
		should_display = false;
	}
}

int main(int argc, char** argv)
{
	if (argc != 2)
	{
		fprintf(stderr, "usage: %s image_file\n", argv[0]);
		return 1;
	}

	auto image_or_error = LibImage::Image::load_from_file(argv[1]);
	if (image_or_error.is_error())
	{
		fprintf(stderr, "Failed to load '%s': %s\n", argv[1], image_or_error.error().get_message());
		return 1;
	}

	auto image = image_or_error.release_value();

	auto window_attributes = LibGUI::Window::default_attributes;
	window_attributes.resizable = true;
	window_attributes.shown = false;

	auto window_or_error = LibGUI::Window::create(s_window_width, s_window_height, argv[1], window_attributes);
	if (window_or_error.is_error())
	{
		fprintf(stderr, "Failed to create an window: %s\n", window_or_error.error().get_message());
		return 1;
	}

	auto window = window_or_error.release_value();

	run(*image, *window);

	return 0;
}
