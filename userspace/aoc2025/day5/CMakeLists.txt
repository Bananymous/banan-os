set(SOURCES
	main.cpp
)

add_executable(aoc2025_day5 ${SOURCES})
banan_include_headers(aoc2025_day5 ban)
banan_link_library(aoc2025_day5 libc)

install(TARGETS aoc2025_day5 OPTIONAL)
