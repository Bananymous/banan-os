set(SOURCES
	main.cpp
)

add_executable(aoc2024_day10 ${SOURCES})
banan_include_headers(aoc2024_day10 ban)
banan_link_library(aoc2024_day10 libc)

install(TARGETS aoc2024_day10 OPTIONAL)
