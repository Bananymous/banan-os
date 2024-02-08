cmake_minimum_required(VERSION 3.26)

project(nslookup CXX)

set(SOURCES
	main.cpp
)

add_executable(nslookup ${SOURCES})
target_compile_options(nslookup PUBLIC -O2 -g)
target_link_libraries(nslookup PUBLIC libc)

add_custom_target(nslookup-install
	COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_BINARY_DIR}/nslookup ${BANAN_BIN}/
	DEPENDS nslookup
)
