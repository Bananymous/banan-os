cmake_minimum_required(VERSION 3.26)

project(Terminal CXX)

set(SOURCES
	main.cpp
	Terminal.cpp
)

add_executable(Terminal ${SOURCES})
target_compile_options(Terminal PUBLIC -O2 -g)
target_link_libraries(Terminal PUBLIC libc ban libfont libgui libinput)

add_custom_target(Terminal-install
	COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_BINARY_DIR}/Terminal ${BANAN_BIN}/
	DEPENDS Terminal
)
