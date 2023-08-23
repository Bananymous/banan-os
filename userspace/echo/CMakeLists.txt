cmake_minimum_required(VERSION 3.26)

project(echo CXX)

set(SOURCES
	main.cpp
)

add_executable(echo ${SOURCES})
target_compile_options(echo PUBLIC -O2 -g)
target_link_libraries(echo PUBLIC libc)

add_custom_target(echo-install
	COMMAND sudo cp ${CMAKE_CURRENT_BINARY_DIR}/echo ${BANAN_BIN}/
	DEPENDS echo
	USES_TERMINAL
)
