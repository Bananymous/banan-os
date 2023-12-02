cmake_minimum_required(VERSION 3.26)

project(aoc2023_day2 CXX)

set(SOURCES
	main.cpp
)

add_executable(aoc2023_day2 ${SOURCES})
target_compile_options(aoc2023_day2 PUBLIC -O2 -g)
target_link_libraries(aoc2023_day2 PUBLIC libc)

add_dependencies(aoc2023_day2 libc-install ban-install)

add_custom_target(aoc2023_day2-install
	COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_BINARY_DIR}/aoc2023_day2 ${BANAN_AOC2023_BIN}/day2
	DEPENDS aoc2023_day2
	DEPENDS aoc2023_always
)

add_dependencies(aoc2023 aoc2023_day2)
add_dependencies(aoc2023-install aoc2023_day2-install)
