set(AOC2024_PROJECTS
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
	full
)

set(BANAN_AOC2024_BIN   ${CMAKE_INSTALL_BINDIR}/aoc2024)
set(BANAN_AOC2024_INPUT ${BANAN_SHARE}/aoc2024)

set(CMAKE_INSTALL_BINDIR ${BANAN_AOC2024_BIN})

add_custom_target(aoc2024)

file(GLOB_RECURSE input_files "input/*")
foreach(file ${input_files})
	install(FILES ${file} DESTINATION ${BANAN_AOC2024_INPUT})
endforeach()

foreach(AOC2024_PROJECT ${AOC2024_PROJECTS})
	add_subdirectory(${AOC2024_PROJECT})
	add_dependencies(aoc2024 aoc2024_${AOC2024_PROJECT})

	target_link_options(aoc2024_${AOC2024_PROJECT} PRIVATE -nolibc)
	target_compile_options(aoc2024_${AOC2024_PROJECT} PRIVATE -g -O2 -Wall -Wextra -Werror)
endforeach()

add_dependencies(userspace aoc2024)
