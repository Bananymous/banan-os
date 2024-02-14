cmake_minimum_required(VERSION 3.26)

project(getopt CXX)

set(SOURCES
	main.cpp
)

add_executable(getopt ${SOURCES})
target_compile_options(getopt PUBLIC -O2 -g)
target_link_libraries(getopt PUBLIC libc ban)

add_custom_target(getopt-install
	COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_BINARY_DIR}/getopt ${BANAN_BIN}/
	DEPENDS getopt
)
