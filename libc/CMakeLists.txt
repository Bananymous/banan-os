cmake_minimum_required(VERSION 3.26)

project(libc CXX)

set(LIBC_SOURCES
	ctype.cpp
	stdio.cpp
	stdlib.cpp
	string.cpp
	unistd.cpp
)

add_custom_target(libc-headers
	COMMAND ${CMAKE_COMMAND} -E copy_directory_if_different ${CMAKE_CURRENT_SOURCE_DIR}/include ${BANAN_INCLUDE}
	DEPENDS sysroot
)

add_library(libc ${LIBC_SOURCES})
add_dependencies(libc headers crt0)

add_custom_target(libc-install
	COMMAND cp ${CMAKE_CURRENT_BINARY_DIR}/libc.a ${BANAN_LIB}/
	DEPENDS libc
)

set(CMAKE_STATIC_LIBRARY_PREFIX "")
