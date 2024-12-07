set(SOURCES
	main.cpp
)

add_executable(aoc2024_day7 ${SOURCES})
banan_link_library(aoc2024_day7 ban)
banan_link_library(aoc2024_day7 libc)

install(TARGETS aoc2024_day7 OPTIONAL)
