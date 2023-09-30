cmake_minimum_required(VERSION 3.26)

project(meminfo CXX)

set(SOURCES
	main.cpp
)

add_executable(meminfo ${SOURCES})
target_compile_options(meminfo PUBLIC -O2 -g)
target_link_libraries(meminfo PUBLIC libc)

add_custom_target(meminfo-install
	COMMAND sudo cp ${CMAKE_CURRENT_BINARY_DIR}/meminfo ${BANAN_BIN}/
	DEPENDS meminfo
	USES_TERMINAL
)
