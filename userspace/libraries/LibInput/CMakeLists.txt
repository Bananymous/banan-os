cmake_minimum_required(VERSION 3.26)

project(libinput CXX)

set(LIBINPUT_SOURCES
	KeyEvent.cpp
	KeyboardLayout.cpp
)

add_custom_target(libinput-headers
	COMMAND ${CMAKE_COMMAND} -E copy_directory_if_different ${CMAKE_CURRENT_SOURCE_DIR}/include/ ${BANAN_INCLUDE}/
	DEPENDS sysroot
)

add_library(libinput ${LIBINPUT_SOURCES})
target_link_libraries(libinput PUBLIC libc ban)

add_custom_target(libinput-install
	COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_CURRENT_BINARY_DIR}/libinput.a ${BANAN_LIB}/
	DEPENDS libinput
	BYPRODUCTS ${BANAN_LIB}/libinput.a
)

set(CMAKE_STATIC_LIBRARY_PREFIX "")
