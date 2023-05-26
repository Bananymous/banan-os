cmake_minimum_required(VERSION 3.26)

project(Shell CXX)

set(SOURCES
	main.cpp
)

add_executable(Shell ${SOURCES})
target_compile_options(Shell PUBLIC -O2 -g)
add_dependencies(Shell libc-install ban-install)
target_link_options(Shell PUBLIC -nodefaultlibs)
target_link_libraries(Shell PUBLIC ${BANAN_LIB}/libc.a)
target_link_libraries(Shell PUBLIC ${BANAN_LIB}/libban.a)

add_custom_target(Shell-install
	COMMAND cp ${CMAKE_CURRENT_BINARY_DIR}/Shell ${BANAN_BIN}/
	DEPENDS Shell
)
