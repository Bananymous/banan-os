set(SOURCES
	main.cpp
)

add_executable(aoc2023_day23 ${SOURCES})
banan_include_headers(aoc2023_day23 ban)
banan_link_library(aoc2023_day23 libc)

install(TARGETS aoc2023_day23 OPTIONAL)
