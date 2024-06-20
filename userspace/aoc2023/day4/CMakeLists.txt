set(SOURCES
	main.cpp
)

add_executable(aoc2023_day4 ${SOURCES})
banan_link_library(aoc2023_day4 ban)
banan_link_library(aoc2023_day4 libc)

install(TARGETS aoc2023_day4 OPTIONAL)
