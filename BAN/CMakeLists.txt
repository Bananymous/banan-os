cmake_minimum_required(VERSION 3.26)

project(BAN CXX)

set(BAN_SOURCES
	BAN/New.cpp
	BAN/String.cpp
	BAN/StringView.cpp
	BAN/Time.cpp
)

add_custom_target(ban-headers
	COMMAND sudo rsync -r ${CMAKE_CURRENT_SOURCE_DIR}/include/ ${BANAN_INCLUDE}/
	DEPENDS sysroot
	USES_TERMINAL
)

add_library(ban ${BAN_SOURCES})
add_dependencies(ban headers libc-install)

add_custom_target(ban-install
	COMMAND sudo cp ${CMAKE_CURRENT_BINARY_DIR}/libban.a ${BANAN_LIB}/
	DEPENDS ban
	BYPRODUCTS ${BANAN_LIB}/libban.a
	USES_TERMINAL
)
