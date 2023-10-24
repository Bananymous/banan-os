cmake_minimum_required(VERSION 3.26)

project(chmod CXX)

set(SOURCES
	main.cpp
)

add_executable(chmod ${SOURCES})
target_compile_options(chmod PUBLIC -O2 -g)
target_link_libraries(chmod PUBLIC libc)

add_custom_target(chmod-install
	COMMAND sudo cp ${CMAKE_CURRENT_BINARY_DIR}/chmod ${BANAN_BIN}/
	DEPENDS chmod
	USES_TERMINAL
)
