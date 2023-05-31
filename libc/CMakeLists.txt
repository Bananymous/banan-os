cmake_minimum_required(VERSION 3.26)

project(libc CXX ASM)

set(LIBC_SOURCES
	assert.cpp
	ctype.cpp
	fcntl.cpp
	printf_impl.cpp
	stdio.cpp
	stdlib.cpp
	string.cpp
	termios.cpp
	unistd.cpp
	math.S
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
	BYPRODUCTS ${BANAN_LIB}/libc.a
)

set(CMAKE_STATIC_LIBRARY_PREFIX "")
