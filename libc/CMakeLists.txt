cmake_minimum_required(VERSION 3.26)

project(libc CXX)

add_custom_target(libc-install
	COMMAND cp -r ${CMAKE_CURRENT_SOURCE_DIR}/include/* ${BANAN_INCLUDE}
	DEPENDS sysroot
)
