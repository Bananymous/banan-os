cmake_minimum_required(VERSION 3.26)

project(Shell CXX)

set(SOURCES
	main.cpp
)

add_executable(Shell ${SOURCES})
target_compile_options(Shell PUBLIC -O2 -g)
target_link_libraries(Shell PUBLIC libc ban)

add_custom_target(Shell-install
	COMMAND cp ${CMAKE_CURRENT_BINARY_DIR}/Shell ${BANAN_BIN}/
	DEPENDS Shell
)
