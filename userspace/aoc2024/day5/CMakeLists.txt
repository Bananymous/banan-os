set(SOURCES
	main.cpp
)

add_executable(aoc2024_day5 ${SOURCES})
banan_include_headers(aoc2024_day5 ban)
banan_link_library(aoc2024_day5 libc)

install(TARGETS aoc2024_day5 OPTIONAL)
