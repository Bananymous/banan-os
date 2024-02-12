cmake_minimum_required(VERSION 3.26)

project(test-tcp CXX)

set(SOURCES
	main.cpp
)

add_executable(test-tcp ${SOURCES})
target_compile_options(test-tcp PUBLIC -O2 -g)
target_link_libraries(test-tcp PUBLIC libc)

add_custom_target(test-tcp-install
	COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_BINARY_DIR}/test-tcp ${BANAN_BIN}/
	DEPENDS test-tcp
)
