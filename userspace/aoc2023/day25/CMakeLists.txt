set(SOURCES
	main.cpp
)

add_executable(aoc2023_day25 ${SOURCES})
banan_link_library(aoc2023_day25 ban)
banan_link_library(aoc2023_day25 libc)

install(TARGETS aoc2023_day25)
