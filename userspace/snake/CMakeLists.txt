cmake_minimum_required(VERSION 3.26)

project(snake CXX)

set(SOURCES
	main.cpp
)

add_executable(snake ${SOURCES})
target_compile_options(snake PUBLIC -O2 -g)
target_link_libraries(snake PUBLIC libc)

add_custom_target(snake-install
	COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_BINARY_DIR}/snake ${BANAN_BIN}/
	DEPENDS snake
)
