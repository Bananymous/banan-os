cmake_minimum_required(VERSION 3.26)

project(aoc2023-full CXX)

set(SOURCES
	main.cpp
)

add_executable(aoc2023-full ${SOURCES})
target_compile_options(aoc2023-full PUBLIC -O2 -g)
target_link_libraries(aoc2023-full PUBLIC libc)

add_custom_target(aoc2023-full-install
	COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_BINARY_DIR}/aoc2023-full ${BANAN_AOC2023_BIN}/full
	DEPENDS aoc2023-full
	DEPENDS aoc2023_always
)

add_dependencies(aoc2023 aoc2023-full)
add_dependencies(aoc2023-install aoc2023-full-install)
