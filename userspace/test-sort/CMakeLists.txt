cmake_minimum_required(VERSION 3.26)

project(test-sort CXX)

set(SOURCES
	main.cpp
)

add_executable(test-sort ${SOURCES})
target_compile_options(test-sort PUBLIC -O2 -g)
target_link_libraries(test-sort PUBLIC libc)

add_custom_target(test-sort-install
	COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_BINARY_DIR}/test-sort ${BANAN_BIN}/
	DEPENDS test-sort
)
