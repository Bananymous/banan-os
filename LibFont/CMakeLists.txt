cmake_minimum_required(VERSION 3.26)

project(libfont CXX)

set(LIBGUI_SOURCES
	Font.cpp
)

add_custom_target(libfont-headers
	COMMAND ${CMAKE_COMMAND} -E copy_directory_if_different ${CMAKE_CURRENT_SOURCE_DIR}/include/ ${BANAN_INCLUDE}/
	DEPENDS sysroot
)

add_library(libfont ${LIBGUI_SOURCES})
add_dependencies(libfont headers libc-install)
target_link_libraries(libfont PUBLIC libc)

add_custom_target(libfont-install
	COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_CURRENT_BINARY_DIR}/libfont.a ${BANAN_LIB}/
	DEPENDS libfont
	BYPRODUCTS ${BANAN_LIB}/libfont.a
)

set(CMAKE_STATIC_LIBRARY_PREFIX "")
