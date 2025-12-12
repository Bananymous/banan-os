set(SOURCES
	main.cpp
)

add_executable(aoc2025_day-template ${SOURCES})
banan_include_headers(aoc2025_day-template ban)
banan_link_library(aoc2025_day-template libc)

install(TARGETS aoc2025_day-template OPTIONAL)
