cmake_minimum_required(VERSION 3.26)

project(aoc2023 CXX)

set(AOC2023_PROJECTS
	day1
)

foreach(AOC2023_PROJECT ${AOC2023_PROJECTS})
	add_subdirectory(${AOC2023_PROJECT})
endforeach()

add_custom_target(aoc2023)
add_custom_target(aoc2023-install DEPENDS aoc2023)

foreach(AOC2023_PROJECT ${AOC2023_PROJECTS})
	target_compile_options(${AOC2023_PROJECT} PRIVATE -g)
	add_dependencies(${AOC2023_PROJECT} libc-install ban-install )
	add_dependencies(aoc2023 ${AOC2023_PROJECT})
	add_dependencies(aoc2023-install ${AOC2023_PROJECT}-install)
endforeach()

add_dependencies(userspace aoc2023)
add_dependencies(userspace-install aoc2023-install)
