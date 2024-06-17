cmake_minimum_required(VERSION 3.26)

project(libimage CXX)

set(LIBIMAGE_SOURCES
	Image.cpp
	Netbpm.cpp
	PNG.cpp
)

add_custom_target(libimage-headers
	COMMAND ${CMAKE_COMMAND} -E copy_directory_if_different ${CMAKE_CURRENT_SOURCE_DIR}/include/ ${BANAN_INCLUDE}/
	DEPENDS sysroot
)

add_library(libimage ${LIBIMAGE_SOURCES})
add_dependencies(libimage headers libc-install)
target_link_libraries(libimage PUBLIC libc)
target_compile_options(libimage PRIVATE -O3)

add_custom_target(libimage-install
	COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_CURRENT_BINARY_DIR}/libimage.a ${BANAN_LIB}/
	DEPENDS libimage
	BYPRODUCTS ${BANAN_LIB}/libimage.a
)

set(CMAKE_STATIC_LIBRARY_PREFIX "")
