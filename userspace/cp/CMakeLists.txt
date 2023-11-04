cmake_minimum_required(VERSION 3.26)

project(cp CXX)

set(SOURCES
	main.cpp
)

add_executable(cp ${SOURCES})
target_compile_options(cp PUBLIC -O2 -g)
target_link_libraries(cp PUBLIC libc ban)

add_custom_target(cp-install
	COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_BINARY_DIR}/cp ${BANAN_BIN}/
	DEPENDS cp
)
