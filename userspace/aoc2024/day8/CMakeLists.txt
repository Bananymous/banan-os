set(SOURCES
	main.cpp
)

add_executable(aoc2024_day8 ${SOURCES})
banan_include_headers(aoc2024_day8 ban)
banan_link_library(aoc2024_day8 libc)

install(TARGETS aoc2024_day8 OPTIONAL)
