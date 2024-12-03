set(SOURCES
	main.cpp
)

add_executable(aoc2024_day3 ${SOURCES})
banan_include_headers(aoc2024_day3 ban)
banan_link_library(aoc2024_day3 libc)

install(TARGETS aoc2024_day3 OPTIONAL)
