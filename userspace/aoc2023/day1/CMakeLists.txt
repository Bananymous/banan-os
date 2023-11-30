cmake_minimum_required(VERSION 3.26)

project(day1 CXX)

set(SOURCES
	main.cpp
)

add_executable(day1 ${SOURCES})
target_compile_options(day1 PUBLIC -O2 -g)
target_link_libraries(day1 PUBLIC libc)

add_custom_target(day1-install
	COMMAND ${CMAKE_COMMAND} -E make_directory ${BANAN_BIN}/aoc2023
	COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_BINARY_DIR}/day1 ${BANAN_BIN}/aoc2023/
	DEPENDS day1
)
