set(SOURCES
	main.cpp
)

add_executable(aoc2024_day15 ${SOURCES})
banan_include_headers(aoc2024_day15 ban)
banan_link_library(aoc2024_day15 libc)

install(TARGETS aoc2024_day15 OPTIONAL)
