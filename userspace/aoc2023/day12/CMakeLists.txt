set(SOURCES
	main.cpp
)

add_executable(aoc2023_day12 ${SOURCES})
banan_link_library(aoc2023_day12 ban)
banan_link_library(aoc2023_day12 libc)

install(TARGETS aoc2023_day12)
