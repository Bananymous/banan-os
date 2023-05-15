cmake_minimum_required(VERSION 3.26)

project(cat CXX)

set(SOURCES
	main.cpp
)

add_executable(cat ${SOURCES})
target_compile_options(cat PUBLIC -O2 -g)
add_dependencies(cat libc-install)
target_link_options(cat PUBLIC -nodefaultlibs -lc)

add_custom_target(cat-install
	COMMAND cp ${CMAKE_CURRENT_BINARY_DIR}/cat ${BANAN_BIN}/
	DEPENDS cat
)
