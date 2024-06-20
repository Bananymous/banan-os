set(SOURCES
	main.cpp
)

add_executable(aoc2023_day-template ${SOURCES})
banan_include_headers(aoc2023_day-template ban)
banan_link_library(aoc2023_day-template libc)

install(TARGETS aoc2023_day-template OPTIONAL)
