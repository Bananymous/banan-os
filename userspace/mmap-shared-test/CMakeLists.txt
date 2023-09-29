cmake_minimum_required(VERSION 3.26)

project(mmap-shared-test CXX)

set(SOURCES
	main.cpp
)

add_executable(mmap-shared-test ${SOURCES})
target_compile_options(mmap-shared-test PUBLIC -O2 -g)
target_link_libraries(mmap-shared-test PUBLIC libc)

add_custom_target(mmap-shared-test-install
	COMMAND sudo cp ${CMAKE_CURRENT_BINARY_DIR}/mmap-shared-test ${BANAN_BIN}/
	DEPENDS mmap-shared-test
	USES_TERMINAL
)
