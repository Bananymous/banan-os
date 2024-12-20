set(SOURCES
	main.cpp
)

add_executable(aoc2024_day20 ${SOURCES})
banan_include_headers(aoc2024_day20 ban)
banan_link_library(aoc2024_day20 libc)

install(TARGETS aoc2024_day20 OPTIONAL)
