cmake_minimum_required(VERSION 3.26)

project(libc CXX ASM)

set(LIBC_SOURCES
	assert.cpp
	ctype.cpp
	dirent.cpp
	fcntl.cpp
	grp.cpp
	malloc.cpp
	printf_impl.cpp
	pwd.cpp
	scanf_impl.cpp
	signal.cpp
	stdio.cpp
	stdlib.cpp
	string.cpp
	strings.cpp
	sys/banan-os.cpp
	sys/mman.cpp
	sys/socket.cpp
	sys/stat.cpp
	sys/wait.cpp
	termios.cpp
	time.cpp
	unistd.cpp
	math.S
	icxxabi.cpp
)

add_custom_target(libc-headers
	COMMAND ${CMAKE_COMMAND} -E copy_directory_if_different ${CMAKE_CURRENT_SOURCE_DIR}/include/ ${BANAN_INCLUDE}/
	DEPENDS sysroot
)

add_custom_target(crtx
	COMMAND ${CMAKE_C_COMPILER} -c ${CMAKE_CURRENT_SOURCE_DIR}/arch/${BANAN_ARCH}/crt0.S -o crt0.o
	COMMAND ${CMAKE_C_COMPILER} -c ${CMAKE_CURRENT_SOURCE_DIR}/arch/${BANAN_ARCH}/crti.S -o crti.o
	COMMAND ${CMAKE_C_COMPILER} -c ${CMAKE_CURRENT_SOURCE_DIR}/arch/${BANAN_ARCH}/crtn.S -o crtn.o
)

add_custom_target(crtx-install
	COMMAND ${CMAKE_COMMAND} -E copy crt0.o ${BANAN_LIB}/
	COMMAND ${CMAKE_COMMAND} -E copy crti.o ${BANAN_LIB}/
	COMMAND ${CMAKE_COMMAND} -E copy crtn.o ${BANAN_LIB}/
	DEPENDS crtx
)

add_library(libc ${LIBC_SOURCES})
add_dependencies(libc headers crtx-install)

target_compile_options(libc PRIVATE -O2 -g -Wstack-usage=512 -fno-tree-loop-distribute-patterns)
target_compile_options(libc PUBLIC -Wall -Wextra -Werror -Wno-error=stack-usage=)

add_custom_target(libc-install
	COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_CURRENT_BINARY_DIR}/libc.a ${BANAN_LIB}/
	DEPENDS libc
	BYPRODUCTS ${BANAN_LIB}/libc.a
)

set(CMAKE_STATIC_LIBRARY_PREFIX "")
