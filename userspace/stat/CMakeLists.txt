cmake_minimum_required(VERSION 3.26)

project(stat CXX)

set(SOURCES
	main.cpp
)

add_executable(stat ${SOURCES})
target_compile_options(stat PUBLIC -O2 -g)
target_link_libraries(stat PUBLIC libc ban)

add_custom_target(stat-install
	COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_BINARY_DIR}/stat ${BANAN_BIN}/
	DEPENDS stat
)
