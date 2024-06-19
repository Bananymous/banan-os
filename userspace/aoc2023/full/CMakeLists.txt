set(SOURCES
	main.cpp
)

add_executable(aoc2023_full ${SOURCES})
banan_include_headers(aoc2023_full ban)
banan_link_library(aoc2023_full libc)

install(TARGETS aoc2023_full)
