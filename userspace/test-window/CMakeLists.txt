cmake_minimum_required(VERSION 3.26)

project(test-window CXX)

set(SOURCES
	main.cpp
)

add_executable(test-window ${SOURCES})
target_compile_options(test-window PUBLIC -O2 -g)
target_link_libraries(test-window PUBLIC libc ban libgui)

add_custom_target(test-window-install
	COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_BINARY_DIR}/test-window ${BANAN_BIN}/
	DEPENDS test-window
)
