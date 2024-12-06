set(SOURCES
	main.cpp
)

add_executable(aoc2024_day6 ${SOURCES})
banan_include_headers(aoc2024_day6 ban)
banan_link_library(aoc2024_day6 libc)

install(TARGETS aoc2024_day6 OPTIONAL)
