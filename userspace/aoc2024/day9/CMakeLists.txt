set(SOURCES
	main.cpp
)

add_executable(aoc2024_day9 ${SOURCES})
banan_include_headers(aoc2024_day9 ban)
banan_link_library(aoc2024_day9 libc)

install(TARGETS aoc2024_day9 OPTIONAL)
