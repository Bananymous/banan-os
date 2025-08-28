#include <LibFont/Font.h>
#include <LibGUI/Window.h>

#include <dirent.h>
#include <fcntl.h>
#include <time.h>

static BAN::ErrorOr<long long> read_integer_from_file(const char* file)
{
	char buffer[128];

	int fd = open(file, O_RDONLY);
	if (fd == -1)
		return BAN::Error::from_errno(errno);
	const ssize_t nread = read(fd, buffer, sizeof(buffer));
	close(fd);

	if (nread < 0)
		return BAN::Error::from_errno(errno);
	if (nread == 0)
		return BAN::Error::from_errno(ENODATA);

	buffer[nread] = '\0';
	return atoll(buffer);
}

static BAN::String get_battery_percentage()
{
	DIR* dirp = opendir("/dev/batteries");
	if (dirp == nullptr)
		return {};

	BAN::String result;
	while (dirent* dirent = readdir(dirp))
	{
		if (strcmp(dirent->d_name, ".") == 0 || strcmp(dirent->d_name, "..") == 0)
			continue;

		char buffer[PATH_MAX + 32];

		sprintf(buffer, "/dev/batteries/%s/capacity_full", dirent->d_name);
		auto cap_full = read_integer_from_file(buffer);
		if (cap_full.is_error() || cap_full.value() == 0)
			continue;

		sprintf(buffer, "/dev/batteries/%s/capacity_now", dirent->d_name);
		auto cap_now = read_integer_from_file(buffer);
		if (cap_now.is_error())
			continue;

		auto string = BAN::String::formatted("{} {}% | ", dirent->d_name, cap_now.value() * 100 / cap_full.value());
		if (string.is_error())
			continue;

		(void)result.append(string.value());
	}

	closedir(dirp);

	return result;
}

static BAN::ErrorOr<BAN::String> get_task_bar_string()
{
	BAN::String result;

	TRY(result.append(get_battery_percentage()));

	const time_t current_time = time(nullptr);
	TRY(result.append(ctime(&current_time)));
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

	window->texture().fill(bg_color);
	window->invalidate();

	bool is_running = true;

	const auto update_time_string =
		[&]()
		{
			auto text_or_error = get_task_bar_string();
			if (text_or_error.is_error())
				return;
			const auto text = text_or_error.release_value();

			const uint32_t text_w = text.size() * font.width();
			const uint32_t text_h = font.height();
			const uint32_t text_x = window->width() - text_w - padding;
			const uint32_t text_y = padding;

			auto& texture = window->texture();
			texture.fill_rect(text_x, text_y, text_w, text_h, bg_color);
			texture.draw_text(text, font, text_x, text_y, fg_color);
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
