set(SOURCES
	main.cpp
)

add_executable(aoc2024_day24 ${SOURCES})
banan_link_library(aoc2024_day24 ban)
banan_link_library(aoc2024_day24 libc)

install(TARGETS aoc2024_day24 OPTIONAL)
