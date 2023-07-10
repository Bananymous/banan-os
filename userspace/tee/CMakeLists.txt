cmake_minimum_required(VERSION 3.26)

project(tee CXX)

set(SOURCES
	main.cpp
)

add_executable(tee ${SOURCES})
target_compile_options(tee PUBLIC -O2 -g)
target_link_libraries(tee PUBLIC libc)

add_custom_target(tee-install
	COMMAND cp ${CMAKE_CURRENT_BINARY_DIR}/tee ${BANAN_BIN}/
	DEPENDS tee
)
