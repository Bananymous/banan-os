set(SOURCES
	main.cpp
)

add_executable(aoc2024_day19 ${SOURCES})
banan_link_library(aoc2024_day19 ban)
banan_link_library(aoc2024_day19 libc)

install(TARGETS aoc2024_day19 OPTIONAL)
