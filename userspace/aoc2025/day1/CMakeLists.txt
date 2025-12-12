set(SOURCES
	main.cpp
)

add_executable(aoc2025_day1 ${SOURCES})
banan_include_headers(aoc2025_day1 ban)
banan_link_library(aoc2025_day1 libc)

install(TARGETS aoc2025_day1 OPTIONAL)
