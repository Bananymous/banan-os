cmake_minimum_required(VERSION 3.26)

project(WindowServer CXX)

set(SOURCES
	main.cpp
	Framebuffer.cpp
	Window.cpp
	WindowServer.cpp
)

add_executable(WindowServer ${SOURCES})
target_compile_options(WindowServer PUBLIC -O2 -g)
target_link_libraries(WindowServer PUBLIC libc ban libfont libgui libinput)

add_custom_target(WindowServer-install
	COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_BINARY_DIR}/WindowServer ${BANAN_BIN}/
	DEPENDS WindowServer
)
