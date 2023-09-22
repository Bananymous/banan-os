cmake_minimum_required(VERSION 3.26)

project(libc CXX ASM)

set(LIBC_SOURCES
	assert.cpp
	ctype.cpp
	dirent.cpp
	fcntl.cpp
	printf_impl.cpp
	pwd.cpp
	signal.cpp
	stdio.cpp
	stdlib.cpp
	string.cpp
	sys/mman.cpp
	sys/stat.cpp
	sys/wait.cpp
	termios.cpp
	time.cpp
	unistd.cpp
	math.S
	icxxabi.cpp
)

add_custom_target(libc-headers
	COMMAND sudo rsync -a ${CMAKE_CURRENT_SOURCE_DIR}/include/ ${BANAN_INCLUDE}/
	DEPENDS sysroot
	USES_TERMINAL
)

add_library(libc ${LIBC_SOURCES})
add_dependencies(libc headers crt0)

target_compile_options(libc PRIVATE -g -Wstack-usage=512)

add_custom_target(libc-install
	COMMAND sudo cp ${CMAKE_CURRENT_BINARY_DIR}/libc.a ${BANAN_LIB}/
	DEPENDS libc
	BYPRODUCTS ${BANAN_LIB}/libc.a
	USES_TERMINAL
)

set(CMAKE_STATIC_LIBRARY_PREFIX "")
