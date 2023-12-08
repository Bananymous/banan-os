cmake_minimum_required(VERSION 3.26)

project(aoc2023 CXX)

set(AOC2023_PROJECTS
	day1
	day2
	day3
	day4
	day5
	day6
	day7
	day8
)

set(BANAN_AOC2023_BIN   ${BANAN_BIN}/aoc2023)
set(BANAN_AOC2023_INPUT ${BANAN_SHARE}/aoc2023)

add_custom_target(aoc2023)
add_custom_target(aoc2023_always
	COMMAND ${CMAKE_COMMAND} -E make_directory ${BANAN_AOC2023_BIN}
	COMMAND ${CMAKE_COMMAND} -E make_directory ${BANAN_AOC2023_INPUT}
	COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/input/* ${BANAN_AOC2023_INPUT}/
)
add_custom_target(aoc2023-install DEPENDS aoc2023 aoc2023_always)

foreach(AOC2023_PROJECT ${AOC2023_PROJECTS})
	add_subdirectory(${AOC2023_PROJECT})
endforeach()

add_dependencies(userspace aoc2023)
add_dependencies(userspace-install aoc2023-install)
