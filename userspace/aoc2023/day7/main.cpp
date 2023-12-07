#include <BAN/Vector.h>
#include <BAN/Sort.h>
#include <BAN/String.h>

#include <ctype.h>
#include <stdio.h>
#include <stdint.h>

using i64 = int64_t;

i64 parse_i64(BAN::StringView part)
{
	i64 result = 0;
	for (char c : part)
	{
		if (!isdigit(c))
			break;
		result = (result * 10) + (c - '0');
	}
	return result;
}

struct Hand
{
	BAN::String hand;
	i64 bid;
};

i64 card_score(char card, bool joker)
{
	if (card == 'A')
		return 14;
	if (card == 'K')
		return 13;
	if (card == 'Q')
		return 12;
	if (card == 'J')
		return joker ? 1 : 11;
	if (card == 'T')
		return 10;

	ASSERT('2' <= card && card <= '9');
	return card - '0';
}

i64 hand_score_no_type(const Hand& hand, bool joker)
{
	ASSERT(hand.hand.size() == 5);
	i64 score = 0;
	for (char c : hand.hand)
		score = (score << 4) | card_score(c, joker);
	ASSERT(score < 1'000'000);
	return score;
}

i64 hand_score(const Hand& hand, bool joker)
{
	ASSERT(hand.hand.size() == 5);

	i64 joker_count = 0;
	i64 cnt[26 + 10] {};
	for (char c : hand.hand)
	{
		if (isdigit(c))
			cnt[c - '0' +  0]++;
		else if (joker && c == 'J')
			joker_count++;
		else
			cnt[c - 'A' + 10]++;
	}

	i64 freq_max1 = 0;
	i64 freq_max2 = 0;
	for (size_t i = 0; i < 36; i++)
	{
		if (cnt[i] > freq_max1)
		{
			freq_max2 = freq_max1;
			freq_max1 = cnt[i];
		}
		else if (cnt[i] > freq_max2)
		{
			freq_max2 = cnt[i];
		}
	}
	freq_max1 += joker_count;

	if (freq_max1 == 5)
		return 6'000'000 + hand_score_no_type(hand, joker);
	if (freq_max1 == 4)
		return 5'000'000 + hand_score_no_type(hand, joker);
	if (freq_max1 == 3 && freq_max2 == 2)
		return 4'000'000 + hand_score_no_type(hand, joker);
	if (freq_max1 == 3)
		return 3'000'000 + hand_score_no_type(hand, joker);
	if (freq_max1 == 2 && freq_max2 == 2)
		return 2'000'000 + hand_score_no_type(hand, joker);
	if (freq_max1 == 2)
		return 1'000'000 + hand_score_no_type(hand, joker);
	return hand_score_no_type(hand, joker);
}

i64 puzzle(FILE* fp, bool joker)
{
	BAN::Vector<Hand> hands;

	char buffer[128];
	while (fgets(buffer, sizeof(buffer), fp))
	{
		BAN::StringView line(buffer);
		if (line.size() < 7)
			continue;
		MUST(hands.emplace_back(
			line.substring(0, 5),
			parse_i64(line.substring(6))
		));
	}

	BAN::sort(hands.begin(), hands.end(),
		[joker] (const Hand& lhs, const Hand& rhs) {
			return hand_score(lhs, joker) < hand_score(rhs, joker);
		}
	);

	i64 score = 0;
	for (size_t i = 0; i < hands.size(); i++)
		score += (i + 1) * hands[i].bid;
	return score;
}

int main(int argc, char** argv)
{
	const char* file_path = "/usr/share/aoc2023/day7_input.txt";

	if (argc >= 2)
		file_path = argv[1];

	FILE* fp = fopen(file_path, "r");
	if (fp == nullptr)
	{
		perror("fopen");
		return 1;
	}

	printf("puzzle1: %lld\n", puzzle(fp, false));

	fseek(fp, 0, SEEK_SET);

	printf("puzzle2: %lld\n", puzzle(fp, true));

	fclose(fp);
}
