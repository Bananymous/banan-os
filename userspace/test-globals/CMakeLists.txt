cmake_minimum_required(VERSION 3.26)

project(test-globals CXX)

set(SOURCES
	main.cpp
)

add_executable(test-globals ${SOURCES})
target_compile_options(test-globals PUBLIC -O2 -g)
target_link_libraries(test-globals PUBLIC libc)

add_custom_target(test-globals-install
	COMMAND sudo cp ${CMAKE_CURRENT_BINARY_DIR}/test-globals ${BANAN_BIN}/
	DEPENDS test-globals
	USES_TERMINAL
)
