set(SOURCES
	main.cpp
)

add_executable(aoc2023_day20 ${SOURCES})
banan_link_library(aoc2023_day20 ban)
banan_link_library(aoc2023_day20 libc)

install(TARGETS aoc2023_day20 OPTIONAL)
