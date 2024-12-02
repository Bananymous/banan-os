set(SOURCES
	main.cpp
)

add_executable(aoc2024_day2 ${SOURCES})
banan_include_headers(aoc2024_day2 ban)
banan_link_library(aoc2024_day2 libc)

install(TARGETS aoc2024_day2 OPTIONAL)
