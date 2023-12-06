cmake_minimum_required(VERSION 3.26)

project(sudo CXX)

set(SOURCES
	main.cpp
)

add_executable(sudo ${SOURCES})
target_compile_options(sudo PUBLIC -O2 -g)
target_link_libraries(sudo PUBLIC libc)

add_custom_target(sudo-install
	COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_BINARY_DIR}/sudo ${BANAN_BIN}/
	COMMAND /bin/chmod u+s ${BANAN_BIN}/sudo
	DEPENDS sudo
)
