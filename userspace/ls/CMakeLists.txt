cmake_minimum_required(VERSION 3.26)

project(ls CXX)

set(SOURCES
	main.cpp
)

add_executable(ls ${SOURCES})
target_compile_options(ls PUBLIC -O2 -g)
target_link_libraries(ls PUBLIC libc ban)

add_custom_target(ls-install
	COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_BINARY_DIR}/ls ${BANAN_BIN}/
	DEPENDS ls
)
