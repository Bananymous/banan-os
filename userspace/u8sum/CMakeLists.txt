cmake_minimum_required(VERSION 3.26)

project(u8sum CXX)

set(SOURCES
	main.cpp
)

add_executable(u8sum ${SOURCES})
target_compile_options(u8sum PUBLIC -O2 -g)
target_link_libraries(u8sum PUBLIC libc)

add_custom_target(u8sum-install
	COMMAND cp ${CMAKE_CURRENT_BINARY_DIR}/u8sum ${BANAN_BIN}/
	DEPENDS u8sum
)
