cmake_minimum_required(VERSION 3.26)

project(dhcp-client CXX)

set(SOURCES
	main.cpp
)

add_executable(dhcp-client ${SOURCES})
target_compile_options(dhcp-client PUBLIC -O2 -g)
target_link_libraries(dhcp-client PUBLIC libc)

add_custom_target(dhcp-client-install
	COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_BINARY_DIR}/dhcp-client ${BANAN_BIN}/
	DEPENDS dhcp-client
)
