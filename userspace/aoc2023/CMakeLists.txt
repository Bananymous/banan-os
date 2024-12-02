set(AOC2023_PROJECTS
	day1
	day2
	day3
	day4
	day5
	day6
	day7
	day8
	day9
	day10
	day11
	day12
	day13
	day14
	day15
	day16
	day17
	day18
	day19
	day20
	day21
	day23
	day24
	day25
	full
)

set(BANAN_AOC2023_BIN   ${CMAKE_INSTALL_BINDIR}/aoc2023)
set(BANAN_AOC2023_INPUT ${BANAN_SHARE}/aoc2023)

set(CMAKE_INSTALL_BINDIR ${BANAN_AOC2023_BIN})

add_custom_target(aoc2023)

file(GLOB_RECURSE input_files "input/*")
foreach(file ${input_files})
	install(FILES ${file} DESTINATION ${BANAN_AOC2023_INPUT})
endforeach()

foreach(AOC2023_PROJECT ${AOC2023_PROJECTS})
	add_subdirectory(${AOC2023_PROJECT})
	add_dependencies(aoc2023 aoc2023_${AOC2023_PROJECT})

	target_link_options(aoc2023_${AOC2023_PROJECT} PRIVATE -nolibc)
	target_compile_options(aoc2023_${AOC2023_PROJECT} PRIVATE -g -O2 -Wall -Wextra -Werror)
endforeach()

add_dependencies(userspace aoc2023)
