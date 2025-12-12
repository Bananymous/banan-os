set(SOURCES
	main.cpp
)

add_executable(aoc2025_full ${SOURCES})
banan_include_headers(aoc2025_full ban)
banan_link_library(aoc2025_full libc)

install(TARGETS aoc2025_full OPTIONAL)
