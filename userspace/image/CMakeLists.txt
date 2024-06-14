cmake_minimum_required(VERSION 3.26)

project(image CXX)

set(SOURCES
	main.cpp
)

add_executable(image ${SOURCES})
target_compile_options(image PUBLIC -O2 -g)
target_link_libraries(image PUBLIC libc ban libimage)

add_custom_target(image-install
	COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_BINARY_DIR}/image ${BANAN_BIN}/
	DEPENDS image
)
