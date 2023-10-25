cmake_minimum_required(VERSION 3.26)

project(rm CXX)

set(SOURCES
	main.cpp
)

add_executable(rm ${SOURCES})
target_compile_options(rm PUBLIC -O2 -g)
target_link_libraries(rm PUBLIC libc)

add_custom_target(rm-install
	COMMAND sudo cp ${CMAKE_CURRENT_BINARY_DIR}/rm ${BANAN_BIN}/
	DEPENDS rm
	USES_TERMINAL
)
