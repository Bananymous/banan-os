cmake_minimum_required(VERSION 3.26)

project(aoc2023_day20 CXX)

set(SOURCES
	main.cpp
)

add_executable(aoc2023_day20 ${SOURCES})
target_compile_options(aoc2023_day20 PUBLIC -O2 -g)
target_link_libraries(aoc2023_day20 PUBLIC libc ban)

add_dependencies(aoc2023_day20 libc-install ban-install)

add_custom_target(aoc2023_day20-install
	COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_BINARY_DIR}/aoc2023_day20 ${BANAN_AOC2023_BIN}/day20
	DEPENDS aoc2023_day20
	DEPENDS aoc2023_always
)

add_dependencies(aoc2023 aoc2023_day20)
add_dependencies(aoc2023-install aoc2023_day20-install)
