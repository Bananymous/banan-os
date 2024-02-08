cmake_minimum_required(VERSION 3.26)

project(resolver CXX)

set(SOURCES
	main.cpp
)

add_executable(resolver ${SOURCES})
target_compile_options(resolver PUBLIC -O2 -g)
target_link_libraries(resolver PUBLIC libc ban)

add_custom_target(resolver-install
	COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_BINARY_DIR}/resolver ${BANAN_BIN}/
	DEPENDS resolver
)
