cmake_minimum_required(VERSION 3.26)

project(test-mouse CXX)

set(SOURCES
	main.cpp
)

add_executable(test-mouse ${SOURCES})
target_compile_options(test-mouse PUBLIC -O2 -g)
target_link_libraries(test-mouse PUBLIC libc)

add_custom_target(test-mouse-install
	COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_BINARY_DIR}/test-mouse ${BANAN_BIN}/
	DEPENDS test-mouse
)
