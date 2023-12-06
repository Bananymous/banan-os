cmake_minimum_required(VERSION 3.26)

project(sleep CXX)

set(SOURCES
	main.cpp
)

add_executable(sleep ${SOURCES})
target_compile_options(sleep PUBLIC -O2 -g)
target_link_libraries(sleep PUBLIC libc)

add_custom_target(sleep-install
	COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_BINARY_DIR}/sleep ${BANAN_BIN}/
	DEPENDS sleep
)
