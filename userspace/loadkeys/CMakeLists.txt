cmake_minimum_required(VERSION 3.26)

project(loadkeys CXX)

set(SOURCES
	main.cpp
)

add_executable(loadkeys ${SOURCES})
target_compile_options(loadkeys PUBLIC -O2 -g)
target_link_libraries(loadkeys PUBLIC libc)

add_custom_target(loadkeys-install
	COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_BINARY_DIR}/loadkeys ${BANAN_BIN}/
	DEPENDS loadkeys
)
