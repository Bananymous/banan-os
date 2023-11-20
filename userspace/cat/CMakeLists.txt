cmake_minimum_required(VERSION 3.26)

project(cat CXX)

set(SOURCES
	main.cpp
)

add_executable(cat ${SOURCES})
target_compile_options(cat PUBLIC -O2 -g)
target_link_libraries(cat PUBLIC libc)

add_custom_target(cat-install
	COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_BINARY_DIR}/cat ${BANAN_BIN}/
	DEPENDS cat
)
