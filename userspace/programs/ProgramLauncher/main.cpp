#include <BAN/HashSet.h>
#include <BAN/Sort.h>
#include <BAN/UTF8.h>
#include <BAN/Vector.h>

#include <LibFont/Font.h>
#include <LibGUI/Window.h>

#include <ctype.h>
#include <dirent.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

static constexpr uint32_t s_line_chars = 64;
static constexpr uint32_t s_list_height = 15;

static constexpr uint32_t s_padding = 3;
static constexpr uint32_t s_margin = 5;

static constexpr uint32_t s_separator_h = 2;

static constexpr uint32_t s_scroll_w = 10;
static constexpr uint32_t s_scroll_h_min = 5;

static constexpr uint32_t s_color_bg1       = 0xCC'404040;
static constexpr uint32_t s_color_bg2       = 0xCC'606060;
static constexpr uint32_t s_color_selected  = 0xCC'0000FF;
static constexpr uint32_t s_color_text      = 0xCC'FFFFFF;
static constexpr uint32_t s_color_separator = 0xCC'FFFFFF;
static constexpr uint32_t s_color_scroll    = 0xCC'808080;

static uint32_t line_w(const LibFont::Font& font)
{
	return s_line_chars * font.width() + 2 * s_padding;
}

static uint32_t line_h(const LibFont::Font& font)
{
	return font.height() + 2 * s_padding;
}

static const BAN::Vector<BAN::String> get_program_list()
{
	BAN::HashSet<BAN::String> paths;
	if (const char* path_env = getenv("PATH"))
	{
		auto path_env_copy = BAN::String(path_env);

		const char* token = strtok(path_env_copy.data(), ":");
		do
			MUST(paths.insert(BAN::StringView(token)));
		while ((token = strtok(nullptr, ":")));
	}

	BAN::HashSet<BAN::String> program_set;
	for (const auto& path : paths)
	{
		DIR* dirp = opendir(path.data());
		if (dirp == nullptr)
			continue;

		dirent* dent;
		while ((dent = readdir(dirp)))
		{
			if (dent->d_type != DT_REG && dent->d_type != DT_LNK)
				continue;

			struct stat st;
			if (fstatat(dirfd(dirp), dent->d_name, &st, 0) == -1)
				continue;
			if (!S_ISREG(st.st_mode))
				continue;
			if (!(st.st_mode & (S_IXUSR | S_IXGRP | S_IXOTH)))
				continue;

			MUST(program_set.insert(BAN::String(dent->d_name)));
		}

		closedir(dirp);
	}

	BAN::Vector<BAN::String> programs;
	MUST(programs.reserve(program_set.size()));
	for (auto& program : program_set)
		MUST(programs.emplace_back(BAN::move(program)));

	BAN::sort::sort(programs.begin(), programs.end(),
		[](const auto& a, const auto& b) -> bool
		{
			const size_t min_size = BAN::Math::min(a.size(), b.size());
			for (size_t i = 0; i < min_size; i++)
				if (a[i] != b[i])
					return a[i] < b[i];
			return a.size() < b.size();
		}
	);

	return programs;
}

static BAN::Vector<BAN::StringView> get_filtered_program_list(BAN::Span<const BAN::String> program_list, BAN::StringView prompt)
{
	BAN::Vector<BAN::StringView> filtered_list;
	for (const auto& program : program_list)
	{
		for (size_t i = 0; i + prompt.size() <= program.size(); i++)
		{
			bool match = true;
			for (size_t j = 0; j < prompt.size() && match; j++)
				if (tolower(prompt[j]) != tolower(program[i + j]))
					match = false;

			if (!match)
				continue;

			MUST(filtered_list.push_back(program.sv()));
			break;
		}
	}

	return filtered_list;
}

void render_search_box(LibGUI::Texture& texture, const LibFont::Font& font, BAN::StringView prompt)
{
	char buffer[s_line_chars + 1];
	snprintf(buffer, sizeof(buffer), "search: %.*s", (int)prompt.size(), prompt.data());

	texture.fill(s_color_bg1);
	texture.draw_text(buffer, font, s_padding, s_padding, s_color_text);
}

void render_list(LibGUI::Texture& texture, const LibFont::Font& font, BAN::Span<BAN::StringView> programs, size_t selected)
{
	texture.fill(s_color_bg1);

	const size_t start = selected / s_list_height * s_list_height;
	const size_t count = BAN::Math::min<size_t>(s_list_height, programs.size() - start);

	const uint32_t line_w = ::line_w(font);
	const uint32_t line_h = ::line_h(font);

	for (size_t i = 0; i < count; i++)
	{
		uint32_t color = (i % 2) ? s_color_bg2 : s_color_bg1;
		if (start + i == selected)
			color = s_color_selected;
		texture.fill_rect(0, line_h * i, line_w, line_h, color);
		texture.draw_text(programs[start + i], font, s_padding, s_padding + line_h * i, s_color_text);
	}
}

void render_scroll(LibGUI::Texture& texture, BAN::Span<BAN::StringView> programs, size_t selected)
{
	texture.fill(s_color_bg1);

	if (programs.empty())
		return;

	texture.fill_rect(
		s_padding,
		texture.height() * selected / programs.size(),
		s_scroll_w,
		BAN::Math::max<uint32_t>(texture.height() / programs.size(), s_scroll_h_min),
		s_color_scroll
	);
}

void render_initial_window(LibGUI::Window& window, const LibFont::Font& font)
{
	auto& texture = window.texture();
	texture.fill(s_color_bg1);
	texture.fill_rect(s_margin, s_margin + line_h(font) + s_padding, line_w(font), s_separator_h, s_color_separator);
}

struct Rectangle { uint32_t x, y, w, h; };

int main()
{
	auto attributes = LibGUI::Window::default_attributes;
	attributes.alpha_channel = true;
	attributes.title_bar = false;

	auto font = MUST(LibFont::Font::load("/usr/share/fonts/lat0-16.psfu"_sv));

	const auto full_program_list = get_program_list();

	// FIXME: implement widgets

	const Rectangle search_area {
		.x = s_margin,
		.y = s_margin,
		.w = line_w(font),
		.h = line_h(font),
	};

	const Rectangle list_area {
		.x = s_margin,
		.y = search_area.x + search_area.h + s_padding + s_separator_h + s_padding,
		.w = line_w(font),
		.h = line_h(font) * s_list_height,
	};

	const Rectangle scroll_area {
		.x = list_area.x + list_area.w - (s_padding + s_scroll_w + s_padding),
		.y = list_area.y,
		.w = s_padding + s_scroll_w + s_padding,
		.h = list_area.h,
	};

	auto search_texture = MUST(LibGUI::Texture::create(
		search_area.w,
		search_area.h,
		s_color_bg1
	));

	auto list_texture = MUST(LibGUI::Texture::create(
		list_area.w,
		list_area.h,
		s_color_bg1
	));

	auto scroll_texture = MUST(LibGUI::Texture::create(
		scroll_area.w,
		scroll_area.h,
		s_color_bg1
	));

	auto window = MUST(LibGUI::Window::create(
		scroll_area.x + scroll_area.w + s_margin,
		scroll_area.y + scroll_area.h + s_margin,
		""_sv,
		attributes
	));

	BAN::String prompt;

	size_t selected = 0;
	auto filtered_list = get_filtered_program_list(full_program_list.span(), prompt);

	const auto refresh_selected =
		[&]()
		{
			render_list(list_texture, font, filtered_list.span(), selected);
			window->texture().copy_texture(list_texture, list_area.x, list_area.y);
			window->invalidate(list_area.x, list_area.y, list_area.w, list_area.h);

			if (filtered_list.size() > s_list_height)
			{
				render_scroll(scroll_texture, filtered_list.span(), selected);
				window->texture().copy_texture(scroll_texture, scroll_area.x, scroll_area.y);
				window->invalidate(scroll_area.x, scroll_area.y, scroll_area.w, scroll_area.h);
			}
		};

	const auto refresh_search =
		[&]()
		{
			selected = 0;
			filtered_list = get_filtered_program_list(full_program_list.span(), prompt);

			render_search_box(search_texture, font, prompt);
			window->texture().copy_texture(search_texture, search_area.x, search_area.y);
			window->invalidate(search_area.x, search_area.y, search_area.w, search_area.h);

			render_list(list_texture, font, filtered_list.span(), selected);
			window->texture().copy_texture(list_texture, list_area.x, list_area.y);
			window->invalidate(list_area.x, list_area.y, list_area.w, list_area.h);

			if (filtered_list.size() > s_list_height)
			{
				render_scroll(scroll_texture, filtered_list.span(), selected);
				window->texture().copy_texture(scroll_texture, scroll_area.x, scroll_area.y);
				window->invalidate(scroll_area.x, scroll_area.y, scroll_area.w, scroll_area.h);
			}
		};

	window->set_key_event_callback(
		[&](LibGUI::EventPacket::KeyEvent::event_t event)
		{
			if (!event.pressed())
				return;

			switch (event.key)
			{
				case LibInput::Key::ArrowUp:
					selected = (selected + filtered_list.size() - 1) % filtered_list.size();
					refresh_selected();
					break;
				case LibInput::Key::ArrowDown:
					selected = (selected + 1) % filtered_list.size();
					refresh_selected();
					break;
				case LibInput::Key::Escape:
					exit(0);
				case LibInput::Key::Enter:
				{
					const char* program = filtered_list.empty() ? prompt.data() : filtered_list[selected].data();

					int null = open("/dev/null", O_RDWR | O_CLOEXEC);
					if (null == -1)
						dwarnln("open: {}", strerror(errno));
					else
					{
						dup2(null, STDIN_FILENO);
						dup2(null, STDOUT_FILENO);
						dup2(null, STDERR_FILENO);
					}

					execlp(program, program, nullptr);
					dwarnln("execlp: {}", strerror(errno));
					exit(1);
				}
				case LibInput::Key::Backspace:
					if (prompt.empty())
						break;
					while (!prompt.empty() && (prompt.back() & 0xC0) == 0x80)
						prompt.pop_back();
					if (!prompt.empty())
						prompt.pop_back();
					refresh_search();
					break;
				default:
					const char* utf8 = LibInput::key_to_utf8(event.key, event.modifier);
					if (utf8 == nullptr)
						break;
					MUST(prompt.append(utf8));
					refresh_search();
					break;
			}
		}
	);

	render_initial_window(*window, font);
	refresh_search();
	window->invalidate();

	for (;;)
	{
		window->wait_events();
		window->poll_events();
	}
}
