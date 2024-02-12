cmake_minimum_required(VERSION 3.26)

project(test-udp CXX)

set(SOURCES
	main.cpp
)

add_executable(test-udp ${SOURCES})
target_compile_options(test-udp PUBLIC -O2 -g)
target_link_libraries(test-udp PUBLIC libc)

add_custom_target(test-udp-install
	COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_BINARY_DIR}/test-udp ${BANAN_BIN}/
	DEPENDS test-udp
)
