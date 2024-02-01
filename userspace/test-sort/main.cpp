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

#define TEST_ALGORITHM(ms, function) do {								\
		uint64_t duration_us = 0;										\
		printf(#function "\n");											\
		for (size_t size = 100; duration_us < ms * 1000; size *= 10) {	\
			BAN::Vector<unsigned> data(size, 0);						\
			for (auto& val : data)										\
				val = rand() % 100;										\
			uint64_t start_ns = CURRENT_NS();							\
			(void)function(data.begin(), data.end());					\
			uint64_t stop_ns = CURRENT_NS();							\
			if (!is_sorted(data)) {										\
				printf("  \e[31mFAILED!\e[m\n");						\
				break;													\
			}															\
			duration_us = (stop_ns - start_ns) / 1'000;					\
			printf("  %5d.%03d ms (%zu)\n",								\
				(int)(duration_us / 1000),								\
				(int)(duration_us % 1000),								\
				size													\
			);															\
		}																\
	} while (0)

int main()
{
	srand(time(0));
	TEST_ALGORITHM(100,  BAN::sort::exchange_sort);
	TEST_ALGORITHM(100,  BAN::sort::quick_sort);
	TEST_ALGORITHM(100,  BAN::sort::insertion_sort);
	TEST_ALGORITHM(100,  BAN::sort::heap_sort);
	TEST_ALGORITHM(100,  BAN::sort::intro_sort);
	TEST_ALGORITHM(1000, BAN::sort::sort);
	TEST_ALGORITHM(1000, BAN::sort::radix_sort);
}
