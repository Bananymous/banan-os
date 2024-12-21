set(SOURCES
	main.cpp
)

add_executable(aoc2024_day21 ${SOURCES})
banan_include_headers(aoc2024_day21 ban)
banan_link_library(aoc2024_day21 libc)

install(TARGETS aoc2024_day21 OPTIONAL)
