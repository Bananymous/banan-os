cmake_minimum_required(VERSION 3.26)

project(LibELF CXX)

add_custom_target(libelf-headers
	COMMAND sudo rsync -r ${CMAKE_CURRENT_SOURCE_DIR}/include/ ${BANAN_INCLUDE}/
	DEPENDS sysroot
	USES_TERMINAL
)

add_custom_target(libelf-install
	DEPENDS libelf-headers
)
