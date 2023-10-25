cmake_minimum_required(VERSION 3.26)

project(mkdir CXX)

set(SOURCES
	main.cpp
)

add_executable(mkdir ${SOURCES})
target_compile_options(mkdir PUBLIC -O2 -g)
target_link_libraries(mkdir PUBLIC libc)

add_custom_target(mkdir-install
	COMMAND sudo cp ${CMAKE_CURRENT_BINARY_DIR}/mkdir ${BANAN_BIN}/
	DEPENDS mkdir
	USES_TERMINAL
)
