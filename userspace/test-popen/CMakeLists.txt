cmake_minimum_required(VERSION 3.26)

project(test-popen CXX)

set(SOURCES
	main.cpp
)

add_executable(test-popen ${SOURCES})
target_compile_options(test-popen PUBLIC -O2 -g)
target_link_libraries(test-popen PUBLIC libc)

add_custom_target(test-popen-install
	COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_BINARY_DIR}/test-popen ${BANAN_BIN}/
	DEPENDS test-popen
)
