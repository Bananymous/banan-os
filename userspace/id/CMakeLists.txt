cmake_minimum_required(VERSION 3.26)

project(id CXX)

set(SOURCES
	main.cpp
)

add_executable(id ${SOURCES})
target_compile_options(id PUBLIC -O2 -g)
target_link_libraries(id PUBLIC libc ban)

add_custom_target(id-install
	COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_BINARY_DIR}/id ${BANAN_BIN}/
	DEPENDS id
)
