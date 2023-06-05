#include <BAN/ScopeGuard.h>
#include <BAN/String.h>
#include <BAN/Vector.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <termios.h>
#include <unistd.h>

struct termios old_termios, new_termios;

BAN::Vector<char*> parse_command(BAN::StringView command)
{
	auto args = MUST(command.split(' '));
	BAN::Vector<char*> result;
	for (auto arg : args)
	{
		char* carg = new char[arg.size() + 1];
		memcpy(carg, arg.data(), arg.size());
		carg[arg.size()] = '\0';
		MUST(result.push_back(carg));
	}
	return result;
}

int execute_command(BAN::StringView command)
{
	auto args = parse_command(command);
	if (args.empty())
		return 0;

	BAN::ScopeGuard deleter([&args] {
		for (char* arg : args)
			delete[] arg;
	});
	
	if (args.front() == "clear"sv)
	{
		fprintf(stdout, "\e[H\e[J");
		fflush(stdout);
		return 0;
	}
	else
	{
		struct stat stat_buf;
		if (stat(args.front(), &stat_buf) == -1)
		{
			fprintf(stderr, "command not found: %s\n", args.front());
			return 1;
		}

		pid_t pid = fork();
		if (pid == 0)
		{
			MUST(args.push_back(nullptr));
			execv(args.front(), args.data());
			perror("execv");
			exit(1);
		}
		if (pid == -1)
		{
			perror("fork()");
			return 1;
		}
		int status;
		if (waitpid(pid, &status, 0) == -1)
			perror("waitpid");
	}

	return 0;
}

int prompt_length(BAN::StringView prompt)
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
			else
				length++;
		}
	}
	return length;
}

int main(int argc, char** argv)
{
	tcgetattr(0, &old_termios);

	new_termios = old_termios;
	new_termios.c_lflag &= ~(ECHO | ICANON);
	tcsetattr(0, TCSANOW, &new_termios);

	BAN::Vector<BAN::String> buffers, history;
	MUST(buffers.emplace_back(""sv));
	size_t index = 0;
	size_t col = 0;

	int waiting_utf8 = 0;

	BAN::String prompt("\e[32muser@host\e[m:\e[34m/\e[m$ "sv);
	fprintf(stdout, "%s", prompt.data());
	fflush(stdout);

	while (true)
	{
		uint8_t ch;
		fread(&ch, 1, sizeof(char), stdin);

		if (waiting_utf8 > 0)
		{
			waiting_utf8--;

			ASSERT((ch & 0xC0) == 0x80);

			fputc(ch, stdout);
			MUST(buffers[index].insert(ch, col++));
			if (waiting_utf8 == 0)
			{
				fprintf(stdout, "\e[s%s\e[u", buffers[index].data() + col);
				fflush(stdout);
			}
			continue;
		}
		else if (ch & 0x80)
		{
			if ((ch & 0xE0) == 0xC0)
				waiting_utf8 = 1;
			else if ((ch & 0xF0) == 0xE0)
				waiting_utf8 = 2;
			else if ((ch & 0xF8) == 0xF0)
				waiting_utf8 = 3;
			else
				ASSERT_NOT_REACHED();
			
			fputc(ch, stdout);
			MUST(buffers[index].insert(ch, col++));
			continue;
		}

		switch (ch)
		{
		case '\e':
			fread(&ch, 1, sizeof(char), stdin);
			if (ch != '[')
				break;
			fread(&ch, 1, sizeof(char), stdin);
			switch (ch)
			{
				case 'A': if (index > 0)					{ index--; col = buffers[index].size(); fprintf(stdout, "\e[%dG%s\e[K", prompt_length(prompt) + 1, buffers[index].data()); fflush(stdout); } break;
				case 'B': if (index < buffers.size() - 1)	{ index++; col = buffers[index].size(); fprintf(stdout, "\e[%dG%s\e[K", prompt_length(prompt) + 1, buffers[index].data()); fflush(stdout); } break;
				case 'C': if (col < buffers[index].size())	{ col++; while ((buffers[index][col - 1] & 0xC0) == 0x80) col++; fprintf(stdout, "\e[C"); fflush(stdout); } break;
				case 'D': if (col > 0)						{ while ((buffers[index][col - 1] & 0xC0) == 0x80) col--; col--; fprintf(stdout, "\e[D"); fflush(stdout); } break;
			}
			break;
		case '\b':
			if (col > 0)
			{
				while ((buffers[index][col - 1] & 0xC0) == 0x80)
					buffers[index].remove(--col);
				buffers[index].remove(--col);
				fprintf(stdout, "\b\e[s%s \e[u", buffers[index].data() + col);
				fflush(stdout);
			}
			break;
		case '\n':
			fputc('\n', stdout);
			if (!buffers[index].empty())
			{
				tcsetattr(0, TCSANOW, &old_termios);
				execute_command(buffers[index]);
				tcsetattr(0, TCSANOW, &new_termios);
				MUST(history.push_back(buffers[index]));
				buffers = history;
				MUST(buffers.emplace_back(""sv));
			}
			fprintf(stdout, "%s", prompt.data());
			fflush(stdout);
			index = buffers.size() - 1;
			col = 0;
			break;
		default:
			MUST(buffers[index].insert(ch, col++));
			fprintf(stdout, "%c\e[s%s\e[u", ch, buffers[index].data() + col);
			fflush(stdout);
			break;
		}
	}

	tcsetattr(0, TCSANOW, &old_termios);
	return 0;
}
