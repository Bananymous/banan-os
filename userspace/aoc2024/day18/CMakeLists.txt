set(SOURCES
	main.cpp
)

add_executable(aoc2024_day18 ${SOURCES})
banan_include_headers(aoc2024_day18 ban)
banan_link_library(aoc2024_day18 libc)

install(TARGETS aoc2024_day18 OPTIONAL)
