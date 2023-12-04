cmake_minimum_required(VERSION 3.26)

project(aoc2023_day4 CXX)

set(SOURCES
	main.cpp
)

add_executable(aoc2023_day4 ${SOURCES})
target_compile_options(aoc2023_day4 PUBLIC -O2 -g)
target_link_libraries(aoc2023_day4 PUBLIC libc ban)

add_dependencies(aoc2023_day4 libc-install ban-install)

add_custom_target(aoc2023_day4-install
	COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_BINARY_DIR}/aoc2023_day4 ${BANAN_AOC2023_BIN}/day4
	DEPENDS aoc2023_day4
	DEPENDS aoc2023_always
)

add_dependencies(aoc2023 aoc2023_day4)
add_dependencies(aoc2023-install aoc2023_day4-install)
