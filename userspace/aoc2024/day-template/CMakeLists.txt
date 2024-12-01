set(SOURCES
	main.cpp
)

add_executable(aoc2024_day-template ${SOURCES})
banan_include_headers(aoc2024_day-template ban)
banan_link_library(aoc2024_day-template libc)

install(TARGETS aoc2024_day-template OPTIONAL)
