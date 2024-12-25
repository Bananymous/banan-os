set(SOURCES
	main.cpp
)

add_executable(aoc2024_day25 ${SOURCES})
banan_include_headers(aoc2024_day25 ban)
banan_link_library(aoc2024_day25 libc)

install(TARGETS aoc2024_day25 OPTIONAL)
