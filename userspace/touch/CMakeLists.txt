cmake_minimum_required(VERSION 3.26)

project(touch CXX)

set(SOURCES
	main.cpp
)

add_executable(touch ${SOURCES})
target_compile_options(touch PUBLIC -O2 -g)
target_link_libraries(touch PUBLIC libc)

add_custom_target(touch-install
	COMMAND sudo cp ${CMAKE_CURRENT_BINARY_DIR}/touch ${BANAN_BIN}/
	DEPENDS touch
	USES_TERMINAL
)
