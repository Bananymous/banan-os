set(SOURCES
	main.cpp
)

add_executable(aoc2024_day14 ${SOURCES})
banan_include_headers(aoc2024_day14 ban)
banan_link_library(aoc2024_day14 libc)

install(TARGETS aoc2024_day14 OPTIONAL)
