#include <BAN/Vector.h>
#include <BAN/Sort.h>

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>

template<typename T>
bool is_sorted(BAN::Vector<T>& vec)
{
	for (size_t i = 0; i < vec.size() - 1; i++)
		if (vec[i] > vec[i + 1])
			return false;
	return true;
}

#define CURRENT_NS() ({ timespec ts; clock_gettime(CLOCK_MONOTONIC, &ts); ts.tv_sec * 1'000'000'000 + ts.tv_nsec; })

#define TEST(name, function, count) do {												\
		BAN::Vector<int> ivec(count, 0);												\
		for (int& i : ivec)																\
			i = rand() % 100;															\
		uint64_t start_ns = CURRENT_NS();												\
		function(ivec.begin(), ivec.end());												\
		uint64_t end_ns = CURRENT_NS();													\
		uint64_t dur_us = (end_ns - start_ns) / 1000;									\
		printf(name " (" #count "): %s\n", is_sorted(ivec) ? "success" : "fail");		\
		printf("  took %" PRIu64 ".%03" PRIu64 " ms\n", dur_us / 1000, dur_us % 1000);	\
	} while (0)

int main()
{
	srand(time(0));
	TEST("exchange sort", BAN::sort::exchange_sort, 100);
	TEST("exchange sort", BAN::sort::exchange_sort, 1000);
	TEST("exchange sort", BAN::sort::exchange_sort, 10000);

	TEST("quick sort", BAN::sort::quick_sort, 100);
	TEST("quick sort", BAN::sort::quick_sort, 1000);
	TEST("quick sort", BAN::sort::quick_sort, 10000);
}
