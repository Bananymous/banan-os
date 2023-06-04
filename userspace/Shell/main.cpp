#include <BAN/ScopeGuard.h>
#include <BAN/String.h>
#include <BAN/Vector.h>
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

	bool got_csi = false;

	BAN::String prompt("\e[32muser@host\e[m:\e[34m/\e[m$ "sv);
	fprintf(stdout, "%s", prompt.data());
	fflush(stdout);

	while (true)
	{
		char c;
		fread(&c, 1, sizeof(char), stdin);

		if (got_csi)
		{
			
			continue;
		}

		switch (c)
		{
		case '\e':
			fread(&c, 1, sizeof(char), stdin);
			if (c != '[')
				break;
			fread(&c, 1, sizeof(char), stdin);
			switch (c)
			{
				case 'A': if (index > 0)					{ index--; col = buffers[index].size(); fprintf(stdout, "\e[G%s\e[K", buffers[index].data()); fflush(stdout); } break;
				case 'B': if (index < buffers.size() - 1)	{ index++; col = buffers[index].size(); fprintf(stdout, "\e[G%s\e[K", buffers[index].data()); fflush(stdout); } break;
				case 'C': break;
				case 'D': break;
			}
			break;
		case '\b':
			if (col > 0)
			{
				buffers[index].pop_back();
				fprintf(stdout, "\b \b");
				fflush(stdout);
				col--;
			}
			break;
		case '\n':
			fputc('\n', stdout);
			if (!buffers[index].empty())
			{
				execute_command(buffers[index]);
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
			MUST(buffers[index].push_back(c));
			fprintf(stdout, "%s \b", buffers[index].data() + col);
			fflush(stdout);
			col++;
			break;
		}
	}

	tcsetattr(0, TCSANOW, &old_termios);
	return 0;
}
