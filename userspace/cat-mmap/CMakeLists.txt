cmake_minimum_required(VERSION 3.26)

project(cat-mmap CXX)

set(SOURCES
	main.cpp
)

add_executable(cat-mmap ${SOURCES})
target_compile_options(cat-mmap PUBLIC -O2 -g)
target_link_libraries(cat-mmap PUBLIC libc)

add_custom_target(cat-mmap-install
	COMMAND sudo cp ${CMAKE_CURRENT_BINARY_DIR}/cat-mmap ${BANAN_BIN}/
	DEPENDS cat-mmap
	USES_TERMINAL
)
