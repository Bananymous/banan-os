cmake_minimum_required(VERSION 3.26)

project(test CXX)

set(TEST_SOURCES
	test.cpp
)

add_executable(test ${TEST_SOURCES})
target_compile_options(test PUBLIC -O2 -g)
target_link_libraries(test PUBLIC libc)

add_custom_target(test-install
	COMMAND sudo cp ${CMAKE_CURRENT_BINARY_DIR}/test ${BANAN_BIN}/
	DEPENDS test
	USES_TERMINAL
)
