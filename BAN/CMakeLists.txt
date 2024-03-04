cmake_minimum_required(VERSION 3.26)

project(BAN CXX)

set(BAN_SOURCES
	BAN/Assert.cpp
	BAN/New.cpp
	BAN/String.cpp
	BAN/StringView.cpp
	BAN/Time.cpp
)

add_custom_target(ban-headers
	COMMAND ${CMAKE_COMMAND} -E copy_directory_if_different ${CMAKE_CURRENT_SOURCE_DIR}/include/ ${BANAN_INCLUDE}/
	DEPENDS sysroot
)

add_library(ban ${BAN_SOURCES})
add_dependencies(ban headers libc-install)

add_custom_target(ban-install
	COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_CURRENT_BINARY_DIR}/libban.a ${BANAN_LIB}/
	DEPENDS ban
	BYPRODUCTS ${BANAN_LIB}/libban.a
)
