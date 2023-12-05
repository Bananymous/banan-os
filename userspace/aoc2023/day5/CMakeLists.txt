cmake_minimum_required(VERSION 3.26)

project(aoc2023_day5 CXX)

set(SOURCES
	main.cpp
)

add_executable(aoc2023_day5 ${SOURCES})
target_compile_options(aoc2023_day5 PUBLIC -O2 -g)
target_link_libraries(aoc2023_day5 PUBLIC libc ban)

add_dependencies(aoc2023_day5 libc-install ban-install)

add_custom_target(aoc2023_day5-install
	COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_BINARY_DIR}/aoc2023_day5 ${BANAN_AOC2023_BIN}/day5
	DEPENDS aoc2023_day5
	DEPENDS aoc2023_always
)

add_dependencies(aoc2023 aoc2023_day5)
add_dependencies(aoc2023-install aoc2023_day5-install)
