cmake_minimum_required(VERSION 3.26)

project(dd CXX)

set(SOURCES
	main.cpp
)

add_executable(dd ${SOURCES})
target_compile_options(dd PUBLIC -O2 -g)
target_link_libraries(dd PUBLIC libc)

add_custom_target(dd-install
	COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_BINARY_DIR}/dd ${BANAN_BIN}/
	DEPENDS dd
)
