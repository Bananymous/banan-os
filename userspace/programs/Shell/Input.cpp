#include "Alias.h"
#include "Builtin.h"
#include "Input.h"

#include <BAN/ScopeGuard.h>
#include <BAN/Sort.h>

#include <ctype.h>
#include <dirent.h>
#include <pwd.h>
#include <sys/stat.h>
#include <unistd.h>

static struct termios s_original_termios;
static struct termios s_raw_termios;
static bool s_termios_initialized { false };

static BAN::Vector<BAN::String> list_matching_entries(BAN::StringView path, BAN::StringView start, bool require_executable)
{
	ASSERT(path.size() < PATH_MAX);

	char path_cstr[PATH_MAX];
	memcpy(path_cstr, path.data(), path.size());
	path_cstr[path.size()] = '\0';

	DIR* dirp = opendir(path_cstr);
	if (dirp == nullptr)
		return {};

	BAN::Vector<BAN::String> result;

	dirent* entry;
	while ((entry = readdir(dirp)))
	{
		if (entry->d_name[0] == '.' && !start.starts_with("."_sv))
			continue;
		if (strncmp(entry->d_name, start.data(), start.size()))
			continue;

		struct stat st;
		if (fstatat(dirfd(dirp), entry->d_name, &st, 0))
			continue;

		if (require_executable)
		{
			if (S_ISDIR(st.st_mode))
				continue;
			if (!(st.st_mode & (S_IXUSR | S_IXGRP | S_IXUSR)))
				continue;
		}

		MUST(result.emplace_back(entry->d_name + start.size()));
		if (S_ISDIR(st.st_mode))
			MUST(result.back().push_back('/'));
	}

	closedir(dirp);

	return BAN::move(result);
}

struct TabCompletion
{
	bool should_escape_spaces { false };
	BAN::StringView prefix;
	BAN::Vector<BAN::String> completions;
};

static TabCompletion list_tab_completion_entries(BAN::StringView current_input)
{
	enum class CompletionType
	{
		Command,
		File,
	};

	BAN::StringView prefix = current_input;
	BAN::String last_argument;
	CompletionType completion_type = CompletionType::Command;

	bool should_escape_spaces = true;
	for (size_t i = 0; i < current_input.size(); i++)
	{
		if (current_input[i] == '\\')
		{
			i++;
			if (i < current_input.size())
				MUST(last_argument.push_back(current_input[i]));
		}
		else if (isspace(current_input[i]) || current_input[i] == ';' || current_input[i] == '|' || current_input.substring(i).starts_with("&&"_sv))
		{
			if (!isspace(current_input[i]))
				completion_type = CompletionType::Command;
			else if (!last_argument.empty())
				completion_type = CompletionType::File;
			if (auto rest = current_input.substring(i); rest.starts_with("||"_sv) || rest.starts_with("&&"_sv))
				i++;
			prefix = current_input.substring(i + 1);
			last_argument.clear();
			should_escape_spaces = true;
		}
		else if (current_input[i] == '\'' || current_input[i] == '"')
		{
			const char quote_type = current_input[i++];
			while (i < current_input.size() && current_input[i] != quote_type)
				MUST(last_argument.push_back(current_input[i++]));
			should_escape_spaces = false;
		}
		else
		{
			MUST(last_argument.push_back(current_input[i]));
		}
	}

	if (last_argument.sv().contains('/'))
		completion_type = CompletionType::File;

	BAN::Vector<BAN::String> result;
	switch (completion_type)
	{
		case CompletionType::Command:
		{
			const char* path_env = getenv("PATH");
			if (path_env)
			{
				auto splitted_path_env = MUST(BAN::StringView(path_env).split(':'));
				for (auto path : splitted_path_env)
				{
					auto matching_entries = list_matching_entries(path, last_argument, true);
					MUST(result.reserve(result.size() + matching_entries.size()));
					for (auto&& entry : matching_entries)
						MUST(result.push_back(BAN::move(entry)));
				}
			}

			Builtin::get().for_each_builtin(
				[&](BAN::StringView name, const Builtin::BuiltinCommand&) -> BAN::Iteration
				{
					if (name.starts_with(last_argument))
						MUST(result.emplace_back(name.substring(last_argument.size())));
					return BAN::Iteration::Continue;
				}
			);

			Alias::get().for_each_alias(
				[&](BAN::StringView name, BAN::StringView) -> BAN::Iteration
				{
					if (name.starts_with(last_argument))
						MUST(result.emplace_back(name.substring(last_argument.size())));
					return BAN::Iteration::Continue;
				}
			);

			break;
		}
		case CompletionType::File:
		{
			BAN::String dir_path;
			if (last_argument.sv().starts_with("/"_sv))
				MUST(dir_path.push_back('/'));
			else
			{
				char cwd_buffer[PATH_MAX];
				if (getcwd(cwd_buffer, sizeof(cwd_buffer)) == nullptr)
					return {};
				MUST(dir_path.reserve(strlen(cwd_buffer) + 1));
				MUST(dir_path.append(cwd_buffer));
				MUST(dir_path.push_back('/'));
			}

			auto match_against = last_argument.sv();
			if (auto idx = match_against.rfind('/'); idx.has_value())
			{
				MUST(dir_path.append(match_against.substring(0, idx.value())));
				match_against = match_against.substring(idx.value() + 1);
			}

			result = list_matching_entries(dir_path, match_against, false);

			break;
		}
	}

	if (auto idx = prefix.rfind('/'); idx.has_value())
		prefix = prefix.substring(idx.value() + 1);

	return { should_escape_spaces, prefix, BAN::move(result) };
}

static int character_length(BAN::StringView prompt)
{
	int length { 0 };
	bool in_escape { false };
	for (char c : prompt)
	{
		if (in_escape)
		{
			if (isalpha(c))
				in_escape = false;
		}
		else
		{
			if (c == '\e')
				in_escape = true;
			else if (((uint8_t)c & 0xC0) != 0x80)
				length++;
		}
	}
	return length;
}

BAN::String Input::parse_ps1_prompt()
{
	const char* raw_prompt = getenv("PS1");
	if (raw_prompt == nullptr)
		return "$ "_sv;

	BAN::String prompt;
	for (int i = 0; raw_prompt[i]; i++)
	{
		char ch = raw_prompt[i];
		if (ch == '\\')
		{
			switch (raw_prompt[++i])
			{
			case 'e':
				MUST(prompt.push_back('\e'));
				break;
			case 'n':
				MUST(prompt.push_back('\n'));
				break;
			case '\\':
				MUST(prompt.push_back('\\'));
				break;
			case '~':
			{
				char buffer[256];
				if (getcwd(buffer, sizeof(buffer)) == nullptr)
					strcpy(buffer, strerrorname_np(errno));

				const char* home = getenv("HOME");
				size_t home_len = home ? strlen(home) : 0;
				if (home && strncmp(buffer, home, home_len) == 0)
				{
					MUST(prompt.push_back('~'));
					MUST(prompt.append(buffer + home_len));
				}
				else
				{
					MUST(prompt.append(buffer));
				}

				break;
			}
			case 'u':
			{
				static char* username = nullptr;
				if (username == nullptr)
				{
					auto* passwd = getpwuid(geteuid());
					if (passwd == nullptr)
						break;
					username = new char[strlen(passwd->pw_name) + 1];
					strcpy(username, passwd->pw_name);
					endpwent();
				}
				MUST(prompt.append(username));
				break;
			}
			case 'h':
			{
				MUST(prompt.append(m_hostname));
				break;
			}
			case '\0':
				MUST(prompt.push_back('\\'));
				break;
			default:
				MUST(prompt.push_back('\\'));
				MUST(prompt.push_back(*raw_prompt));
				break;
			}
		}
		else
		{
			MUST(prompt.push_back(ch));
		}
	}

	return prompt;
}

BAN::Optional<BAN::String> Input::get_input(BAN::Optional<BAN::StringView> custom_prompt)
{
	tcsetattr(0, TCSANOW, &s_raw_termios);
	BAN::ScopeGuard _([] { tcsetattr(0, TCSANOW, &s_original_termios); });

	BAN::String ps1_prompt;
	if (!custom_prompt.has_value())
		ps1_prompt = parse_ps1_prompt();

	const auto print_prompt =
		[&]()
		{
			if (custom_prompt.has_value())
				printf("%.*s", (int)custom_prompt->size(), custom_prompt->data());
			else
				printf("%.*s", (int)ps1_prompt.size(), ps1_prompt.data());
		};
	const auto prompt_length =
		[&]() -> int
		{
			if (custom_prompt.has_value())
				return custom_prompt->size();
			return character_length(ps1_prompt);
		};

	print_prompt();
	fflush(stdout);

	while (true)
	{
		int chi = getchar();
		if (chi == EOF)
		{
			if (errno != EINTR)
			{
				perror("getchar");
				exit(1);
			}

			clearerr(stdin);
			m_buffers = m_history;
			MUST(m_buffers.emplace_back(""_sv));
			m_buffer_index = m_buffers.size() - 1;
			m_buffer_col = 0;
			putchar('\n');
			print_prompt();
			fflush(stdout);
			continue;
		}

		uint8_t ch = chi;
		if (ch != '\t')
		{
			m_tab_completions.clear();
			m_tab_index.clear();
		}

		if (m_waiting_utf8 > 0)
		{
			m_waiting_utf8--;

			ASSERT((ch & 0xC0) == 0x80);

			putchar(ch);
			MUST(m_buffers[m_buffer_index].insert(ch, m_buffer_col++));
			if (m_waiting_utf8 == 0)
			{
				printf("\e[s%s\e[u", m_buffers[m_buffer_index].data() + m_buffer_col);
				fflush(stdout);
			}
			continue;
		}
		else if (ch & 0x80)
		{
			if ((ch & 0xE0) == 0xC0)
				m_waiting_utf8 = 1;
			else if ((ch & 0xF0) == 0xE0)
				m_waiting_utf8 = 2;
			else if ((ch & 0xF8) == 0xF0)
				m_waiting_utf8 = 3;
			else
				ASSERT_NOT_REACHED();

			putchar(ch);
			MUST(m_buffers[m_buffer_index].insert(ch, m_buffer_col++));
			continue;
		}

		switch (ch)
		{
		case '\e':
		{
			ch = getchar();
			if (ch != '[')
				break;
			ch = getchar();

			int value = 0;
			while (isdigit(ch))
			{
				value = (value * 10) + (ch - '0');
				ch = getchar();
			}

			switch (ch)
			{
				case 'A':
					if (m_buffer_index > 0)
					{
						m_buffer_index--;
						m_buffer_col = m_buffers[m_buffer_index].size();
						printf("\e[%dG%s\e[K", prompt_length() + 1, m_buffers[m_buffer_index].data());
						fflush(stdout);
					}
					break;
				case 'B':
					if (m_buffer_index < m_buffers.size() - 1)
					{
						m_buffer_index++;
						m_buffer_col = m_buffers[m_buffer_index].size();
						printf("\e[%dG%s\e[K", prompt_length() + 1, m_buffers[m_buffer_index].data());
						fflush(stdout);
					}
					break;
				case 'C':
					if (m_buffer_col < m_buffers[m_buffer_index].size())
					{
						m_buffer_col++;
						while ((m_buffers[m_buffer_index][m_buffer_col - 1] & 0xC0) == 0x80)
							m_buffer_col++;
						printf("\e[C");
						fflush(stdout);
					}
					break;
				case 'D':
					if (m_buffer_col > 0)
					{
						while ((m_buffers[m_buffer_index][m_buffer_col - 1] & 0xC0) == 0x80)
							m_buffer_col--;
						m_buffer_col--;
						printf("\e[D");
						fflush(stdout);
					}
					break;
				case '~':
					switch (value)
					{
						case 3: // delete
							if (m_buffer_col >= m_buffers[m_buffer_index].size())
								break;
							m_buffers[m_buffer_index].remove(m_buffer_col);
							while (m_buffer_col < m_buffers[m_buffer_index].size() && (m_buffers[m_buffer_index][m_buffer_col] & 0xC0) == 0x80)
								m_buffers[m_buffer_index].remove(m_buffer_col);
							printf("\e[s%s \e[u", m_buffers[m_buffer_index].data() + m_buffer_col);
							fflush(stdout);
							break;
					}
					break;
			}
			break;
		}
		case '\x0C': // ^L
		{
			int x = prompt_length() + character_length(m_buffers[m_buffer_index].sv().substring(m_buffer_col)) + 1;
			printf("\e[H\e[J");
			print_prompt();
			printf("%s\e[u\e[1;%dH", m_buffers[m_buffer_index].data(), x);
			fflush(stdout);
			break;
		}
		case '\b':
			if (m_buffer_col <= 0)
				break;
			while ((m_buffers[m_buffer_index][m_buffer_col - 1] & 0xC0) == 0x80)
				m_buffer_col--;
			m_buffer_col--;
			printf("\e[D");
			fflush(stdout);
			break;
		case '\x01': // ^A
			m_buffer_col = 0;
			printf("\e[%dG", prompt_length() + 1);
			fflush(stdout);
			break;
		case '\x03': // ^C
			putchar('\n');
			print_prompt();
			fflush(stdout);
			m_buffers[m_buffer_index].clear();
			m_buffer_col = 0;
			break;
		case '\x04': // ^D
			if (!m_buffers[m_buffer_index].empty())
				break;
			putchar('\n');
			return {};
		case '\x7F': // backspace
			if (m_buffer_col <= 0)
				break;
			while ((m_buffers[m_buffer_index][m_buffer_col - 1] & 0xC0) == 0x80)
				m_buffers[m_buffer_index].remove(--m_buffer_col);
			m_buffers[m_buffer_index].remove(--m_buffer_col);
			printf("\b\e[s%s \e[u", m_buffers[m_buffer_index].data() + m_buffer_col);
			fflush(stdout);
			break;
		case '\n':
		{
			BAN::String input;
			MUST(input.append(m_buffers[m_buffer_index]));

			if (!m_buffers[m_buffer_index].empty())
			{
				MUST(m_history.push_back(m_buffers[m_buffer_index]));
				m_buffers = m_history;
				MUST(m_buffers.emplace_back(""_sv));
			}
			m_buffer_index = m_buffers.size() - 1;
			m_buffer_col = 0;
			putchar('\n');

			return input;
		}
		case '\t':
		{
			// FIXME: tab completion is really hacked together currently.
			//        this should ask token parser about the current parse state
			//        and do completions based on that, not raw strings

			if (m_buffer_col != m_buffers[m_buffer_index].size())
				continue;

			if (m_tab_completions.has_value())
			{
				ASSERT(m_tab_completions->size() >= 2);

				if (!m_tab_index.has_value())
					m_tab_index = 0;
				else
				{
					MUST(m_buffers[m_buffer_index].resize(m_tab_completion_keep));
					m_buffer_col = m_tab_completion_keep;
					*m_tab_index = (*m_tab_index + 1) % m_tab_completions->size();
				}

				MUST(m_buffers[m_buffer_index].append(m_tab_completions.value()[*m_tab_index]));
				m_buffer_col += m_tab_completions.value()[*m_tab_index].size();

				printf("\e[%dG%s\e[K", prompt_length() + 1, m_buffers[m_buffer_index].data());
				fflush(stdout);

				break;
			}

			m_tab_completion_keep = m_buffer_col;
			auto [should_escape_spaces, prefix, completions] = list_tab_completion_entries(m_buffers[m_buffer_index].sv().substring(0, m_tab_completion_keep));

			BAN::sort::sort(completions.begin(), completions.end(),
				[](const BAN::String& a, const BAN::String& b) {
					if (auto cmp = strcmp(a.data(), b.data()))
						return cmp < 0;
					return a.size() < b.size();
				}
			);

			for (size_t i = 1; i < completions.size();)
			{
				if (completions[i - 1] == completions[i])
					completions.remove(i);
				else
					i++;
			}

			if (completions.empty())
				break;

			size_t all_match_len = 0;
			for (;;)
			{
				if (completions.front().size() <= all_match_len)
					break;
				const char target = completions.front()[all_match_len];

				bool all_matched = true;
				for (const auto& completion : completions)
				{
					if (completion.size() > all_match_len && completion[all_match_len] == target)
						continue;
					all_matched = false;
					break;
				}

				if (!all_matched)
					break;
				all_match_len++;
			}

			if (all_match_len)
			{
				auto completion = completions.front().sv().substring(0, all_match_len);

				BAN::String temp_escaped;
				if (should_escape_spaces)
				{
					MUST(temp_escaped.append(completion));
					for (size_t i = 0; i < temp_escaped.size(); i++)
					{
						if (!isspace(temp_escaped[i]))
							continue;
						MUST(temp_escaped.insert('\\', i));
						i++;
					}
					completion = temp_escaped.sv();

					if (!m_buffers[m_buffer_index].empty() && m_buffers[m_buffer_index].back() == '\\' && completion.front() == '\\')
						completion = completion.substring(1);
				}

				m_buffer_col += completion.size();
				MUST(m_buffers[m_buffer_index].append(completion));
				printf("%.*s", (int)completion.size(), completion.data());
				fflush(stdout);
				break;
			}

			if (completions.size() == 1)
			{
				ASSERT(all_match_len == completions.front().size());
				break;
			}

			printf("\n");
			for (size_t i = 0; i < completions.size(); i++)
			{
				if (i != 0)
					printf(" ");
				const char* format = completions[i].sv().contains(' ') ? "'%.*s%s'" : "%.*s%s";
				printf(format, (int)prefix.size(), prefix.data(), completions[i].data());
			}
			printf("\n");
			print_prompt();
			printf("%s", m_buffers[m_buffer_index].data());
			fflush(stdout);

			if (should_escape_spaces)
			{
				for (auto& completion : completions)
				{
					for (size_t i = 0; i < completion.size(); i++)
					{
						if (!isspace(completion[i]))
							continue;
						MUST(completion.insert('\\', i));
						i++;
					}
				}
			}

			m_tab_completion_keep = m_buffer_col;
			m_tab_completions = BAN::move(completions);

			break;
		}
		default:
			MUST(m_buffers[m_buffer_index].insert(ch, m_buffer_col++));
			if (m_buffer_col == m_buffers[m_buffer_index].size())
				putchar(ch);
			else
				printf("%c\e[s%s\e[u", ch, m_buffers[m_buffer_index].data() + m_buffer_col);
			fflush(stdout);
			break;
		}
	}
}

Input::Input()
{
	if (!s_termios_initialized)
	{
		tcgetattr(0, &s_original_termios);
		s_raw_termios = s_original_termios;
		s_raw_termios.c_lflag &= ~(ECHO | ICANON);
		atexit([] { tcsetattr(0, TCSANOW, &s_original_termios); });
		s_termios_initialized = true;
	}

	char hostname_buffer[HOST_NAME_MAX];
	if (gethostname(hostname_buffer, sizeof(hostname_buffer)) == 0) {
		MUST(m_hostname.append(hostname_buffer));
	}
}
