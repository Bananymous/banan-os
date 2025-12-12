set(SOURCES
	main.cpp
)

add_executable(aoc2025_day2 ${SOURCES})
banan_include_headers(aoc2025_day2 ban)
banan_link_library(aoc2025_day2 libc)

install(TARGETS aoc2025_day2 OPTIONAL)
