cmake_minimum_required(VERSION 3.26)

project(init CXX)

set(SOURCES
	main.cpp
)

add_executable(init ${SOURCES})
target_compile_options(init PUBLIC -O2 -g)
target_link_libraries(init PUBLIC libc ban)

add_custom_target(init-install
	COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_BINARY_DIR}/init ${BANAN_BIN}/
	DEPENDS init
)
