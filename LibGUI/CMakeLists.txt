cmake_minimum_required(VERSION 3.26)

project(libgui CXX)

set(LIBGUI_SOURCES
	Window.cpp
)

add_custom_target(libgui-headers
	COMMAND ${CMAKE_COMMAND} -E copy_directory_if_different ${CMAKE_CURRENT_SOURCE_DIR}/include/ ${BANAN_INCLUDE}/
	DEPENDS sysroot
)

add_library(libgui ${LIBGUI_SOURCES})
add_dependencies(libgui headers libc-install)
target_link_libraries(libgui PUBLIC libc libfont)

add_custom_target(libgui-install
	COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_CURRENT_BINARY_DIR}/libgui.a ${BANAN_LIB}/
	DEPENDS libgui
	BYPRODUCTS ${BANAN_LIB}/libgui.a
)

set(CMAKE_STATIC_LIBRARY_PREFIX "")
