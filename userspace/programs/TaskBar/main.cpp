#include <LibFont/Font.h>
#include <LibGUI/Window.h>

#include <time.h>

static BAN::String get_task_bar_string()
{
	BAN::String result;

	const time_t current_time = time(nullptr);
	if (!result.append(ctime(&current_time)).is_error())
		result.pop_back();

	return result;
}

int main()
{
	constexpr uint32_t padding = 3;
	constexpr uint32_t bg_color = 0xFF202020;
	constexpr uint32_t fg_color = 0xFFFFFFFF;

	auto font = MUST(LibFont::Font::load("/usr/share/fonts/lat0-16.psfu"_sv));

	auto attributes = LibGUI::Window::default_attributes;
	attributes.title_bar = false;
	attributes.movable = false;
	attributes.focusable = false;
	attributes.alpha_channel = false;
	attributes.rounded_corners = false;

	auto window = MUST(LibGUI::Window::create(0, font.height() + 2 * padding, "TaskBar", attributes));

	window->set_close_window_event_callback([]() {});

	window->set_position(0, 0);

	window->fill(bg_color);
	window->invalidate();

	bool is_running = true;

	const auto update_time_string =
		[&]()
		{
			const auto text = get_task_bar_string();

			const uint32_t text_w = text.size() * font.width();
			const uint32_t text_h = font.height();
			const uint32_t text_x = window->width() - text_w - padding;
			const uint32_t text_y = padding;

			window->fill_rect(text_x, text_y, text_w, text_h, bg_color);
			window->draw_text(text, font, text_x, text_y, fg_color);
			window->invalidate(text_x, text_y, text_w, text_h);
		};

	while (is_running)
	{
		update_time_string();

		constexpr uint64_t ns_per_s = 1'000'000'000;

		timespec current_ts;
		clock_gettime(CLOCK_REALTIME, &current_ts);

		uint64_t current_ns = 0;
		current_ns += current_ts.tv_sec * ns_per_s;
		current_ns += current_ts.tv_nsec;

		uint64_t target_ns = current_ns;
		if (auto rem = target_ns % ns_per_s)
			target_ns += ns_per_s - rem;

		uint64_t sleep_ns = target_ns - current_ns;

		timespec sleep_ts;
		sleep_ts.tv_sec  = sleep_ns / ns_per_s;
		sleep_ts.tv_nsec = sleep_ns % ns_per_s;

		nanosleep(&sleep_ts, nullptr);
	}
}
