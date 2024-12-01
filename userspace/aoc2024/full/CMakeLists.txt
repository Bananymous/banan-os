set(SOURCES
	main.cpp
)

add_executable(aoc2024_full ${SOURCES})
banan_include_headers(aoc2024_full ban)
banan_link_library(aoc2024_full libc)

install(TARGETS aoc2024_full OPTIONAL)
