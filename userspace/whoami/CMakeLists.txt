cmake_minimum_required(VERSION 3.26)

project(whoami CXX)

set(SOURCES
	main.cpp
)

add_executable(whoami ${SOURCES})
target_compile_options(whoami PUBLIC -O2 -g)
target_link_libraries(whoami PUBLIC libc ban)

add_custom_target(whoami-install
	COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_BINARY_DIR}/whoami ${BANAN_BIN}/
	DEPENDS whoami
)
