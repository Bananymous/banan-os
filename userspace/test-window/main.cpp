#include <BAN/Debug.h>

#include <LibGUI/Window.h>

#include <stdlib.h>
#include <unistd.h>

void randomize_color(BAN::UniqPtr<LibGUI::Window>& window)
{
	uint32_t color = ((rand() % 255) << 16) | ((rand() % 255) << 8) | ((rand() % 255) << 0);
	for (uint32_t y = 0; y < window->height(); y++)
		for (uint32_t x = 0; x < window->width(); x++)
			window->set_pixel(x, y, color);
	window->invalidate();
}

int main()
{
	timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	srand(ts.tv_nsec);

	auto window_or_error = LibGUI::Window::create(300, 200);
	if (window_or_error.is_error())
	{
		dprintln("{}", window_or_error.error());
		return 1;
	}

	auto window = window_or_error.release_value();
	window->set_mouse_button_event_callback(
		[&](LibGUI::EventPacket::MouseButtonEvent event)
		{
			if (event.pressed && event.button == LibGUI::EventPacket::MouseButton::Left)
				randomize_color(window);

			const char* button;
			switch (event.button)
			{
				case LibGUI::EventPacket::MouseButton::Left: button = "left"; break;
				case LibGUI::EventPacket::MouseButton::Right: button = "right"; break;
				case LibGUI::EventPacket::MouseButton::Middle: button = "middle"; break;
				case LibGUI::EventPacket::MouseButton::Extra1: button = "extra1"; break;
				case LibGUI::EventPacket::MouseButton::Extra2: button = "extra2"; break;
			}
			dprintln("mouse button '{}' {} at {}, {}", button, event.pressed ? "pressed" : "released", event.x, event.y);
		}
	);

	randomize_color(window);

	for (;;)
	{
		window->poll_events();

		timespec duration;
		duration.tv_sec = 0;
		duration.tv_nsec = 16'666'666;
		nanosleep(&duration, nullptr);
	}
}
