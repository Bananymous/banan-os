set(SOURCES
	main.cpp
)

add_executable(aoc2025_day8 ${SOURCES})
banan_include_headers(aoc2025_day8 ban)
banan_link_library(aoc2025_day8 libc)

install(TARGETS aoc2025_day8 OPTIONAL)
