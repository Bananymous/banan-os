cmake_minimum_required(VERSION 3.26)

project(BAN CXX)

add_custom_target(ban-headers
	COMMAND ${CMAKE_COMMAND} -E copy_directory_if_different ${CMAKE_CURRENT_SOURCE_DIR}/include ${BANAN_INCLUDE}
	DEPENDS sysroot
)

add_custom_target(ban-install
	DEPENDS ban-headers
)

