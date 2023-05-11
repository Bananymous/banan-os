cmake_minimum_required(VERSION 3.26)

project(yes CXX)

set(SOURCES
	main.cpp
)

add_executable(yes ${SOURCES})
target_compile_options(yes PUBLIC -O2 -g)
add_dependencies(yes libc-install)
target_link_options(yes PUBLIC -nodefaultlibs -lc)

add_custom_target(yes-install
	COMMAND cp ${CMAKE_CURRENT_BINARY_DIR}/yes ${BANAN_BIN}/
	DEPENDS yes
)
