cmake_minimum_required(VERSION 3.26)

project(test-unix-socket CXX)

set(SOURCES
	main.cpp
)

add_executable(test-unix-socket ${SOURCES})
target_compile_options(test-unix-socket PUBLIC -O2 -g)
target_link_libraries(test-unix-socket PUBLIC libc)

add_custom_target(test-unix-socket-install
	COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_BINARY_DIR}/test-unix-socket ${BANAN_BIN}/
	DEPENDS test-unix-socket
)
