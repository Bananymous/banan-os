#include <BAN/Vector.h>
#include <BAN/String.h>
#include <BAN/StringView.h>

#include <stdio.h>
#include <string.h>

static int matching_numbers(BAN::StringView line)
{
	ASSERT(line.size() == 117);
	line = line.substring(0, line.size() - 1);

	auto winning_numbers = MUST(line.substring(10, 29).split(' '));
	auto your_numbers = MUST(line.substring(42).split(' '));

	int matching = 0;
	for (auto number : your_numbers)
		if (winning_numbers.contains(number))
			matching++;

	return matching;
}

int puzzle1(FILE* fp)
{
	int result = 0;

	char buffer[128];
	while (fgets(buffer, sizeof(buffer), fp))
	{
		if (strncmp(buffer, "Card", 4))
			continue;

		int matching = matching_numbers(buffer);
		if (matching > 0)
			result += 1 << (matching - 1);
	}

	return result;
}

int puzzle2(FILE* fp)
{
	struct Card
	{
		BAN::String line;
		int count;
	};

	BAN::Vector<Card> cards;

	char buffer[128];
	while (fgets(buffer, sizeof(buffer), fp))
		if (strncmp(buffer, "Card", 4) == 0)
			MUST(cards.emplace_back(BAN::StringView(buffer), 1));

	int result = 0;
	for (size_t i = 0; i < cards.size(); i++)
	{
		int matching = matching_numbers(cards[i].line);
		for (int j = 0; j < matching; j++)
			cards[i + j + 1].count += cards[i].count;
		result += cards[i].count;
	}

	return result;
}

int main(int argc, char** argv)
{
	const char* file_path = "/usr/share/aoc2023/day4_input.txt";

	if (argc >= 2)
		file_path = argv[1];

	FILE* fp = fopen(file_path, "r");
	if (fp == nullptr)
	{
		perror("fopen");
		return 1;
	}

	printf("puzzle1: %d\n", puzzle1(fp));

	fseek(fp, 0, SEEK_SET);

	printf("puzzle2: %d\n", puzzle2(fp));

	fclose(fp);
}
