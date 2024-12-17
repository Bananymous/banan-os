set(SOURCES
	main.cpp
)

add_executable(aoc2024_day17 ${SOURCES})
banan_include_headers(aoc2024_day17 ban)
banan_link_library(aoc2024_day17 libc)

install(TARGETS aoc2024_day17 OPTIONAL)
