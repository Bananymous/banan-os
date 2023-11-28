cmake_minimum_required(VERSION 3.26)

project(test-framebuffer CXX)

set(SOURCES
	main.cpp
)

add_executable(test-framebuffer ${SOURCES})
target_compile_options(test-framebuffer PUBLIC -O2 -g)
target_link_libraries(test-framebuffer PUBLIC libc)

add_custom_target(test-framebuffer-install
	COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_BINARY_DIR}/test-framebuffer ${BANAN_BIN}/
	DEPENDS test-framebuffer
)
