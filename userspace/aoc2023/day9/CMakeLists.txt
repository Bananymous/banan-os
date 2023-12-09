cmake_minimum_required(VERSION 3.26)

project(aoc2023_day9 CXX)

set(SOURCES
	main.cpp
)

add_executable(aoc2023_day9 ${SOURCES})
target_compile_options(aoc2023_day9 PUBLIC -O2 -g)
target_link_libraries(aoc2023_day9 PUBLIC libc ban)

add_dependencies(aoc2023_day9 libc-install ban-install)

add_custom_target(aoc2023_day9-install
	COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_BINARY_DIR}/aoc2023_day9 ${BANAN_AOC2023_BIN}/day9
	DEPENDS aoc2023_day9
	DEPENDS aoc2023_always
)

add_dependencies(aoc2023 aoc2023_day9)
add_dependencies(aoc2023-install aoc2023_day9-install)
