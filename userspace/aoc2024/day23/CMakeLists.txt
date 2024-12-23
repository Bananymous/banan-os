set(SOURCES
	main.cpp
)

add_executable(aoc2024_day23 ${SOURCES})
banan_link_library(aoc2024_day23 ban)
banan_link_library(aoc2024_day23 libc)

install(TARGETS aoc2024_day23 OPTIONAL)
