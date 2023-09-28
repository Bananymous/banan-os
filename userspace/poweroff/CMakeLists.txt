cmake_minimum_required(VERSION 3.26)

project(poweroff CXX)

set(SOURCES
	main.cpp
)

add_executable(poweroff ${SOURCES})
target_compile_options(poweroff PUBLIC -O2 -g)
target_link_libraries(poweroff PUBLIC libc)

add_custom_target(poweroff-install
	COMMAND sudo cp ${CMAKE_CURRENT_BINARY_DIR}/poweroff ${BANAN_BIN}/
	DEPENDS poweroff
	USES_TERMINAL
)
