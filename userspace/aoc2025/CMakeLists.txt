set(AOC2025_PROJECTS
	day1
	day2
	day3
	day4
	day5
	day6
	day7
	full
)

set(BANAN_AOC2025_BIN   ${CMAKE_INSTALL_BINDIR}/aoc2025)
set(BANAN_AOC2025_INPUT ${BANAN_SHARE}/aoc2025)

set(CMAKE_INSTALL_BINDIR ${BANAN_AOC2025_BIN})

add_custom_target(aoc2025)

file(GLOB_RECURSE input_files "input/*")
foreach(file ${input_files})
	install(FILES ${file} DESTINATION ${BANAN_AOC2025_INPUT})
endforeach()

foreach(AOC2025_PROJECT ${AOC2025_PROJECTS})
	add_subdirectory(${AOC2025_PROJECT})
	add_dependencies(aoc2025 aoc2025_${AOC2025_PROJECT})

	target_link_options(aoc2025_${AOC2025_PROJECT} PRIVATE -nolibc)
	target_compile_options(aoc2025_${AOC2025_PROJECT} PRIVATE -g -O2 -Wall -Wextra -Werror)
endforeach()

add_dependencies(userspace aoc2025)
