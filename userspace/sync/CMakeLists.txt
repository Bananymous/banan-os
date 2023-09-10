cmake_minimum_required(VERSION 3.26)

project(sync CXX)

set(SOURCES
	main.cpp
)

add_executable(sync ${SOURCES})
target_compile_options(sync PUBLIC -O2 -g)
target_link_libraries(sync PUBLIC libc)

add_custom_target(sync-install
	COMMAND sudo cp ${CMAKE_CURRENT_BINARY_DIR}/sync ${BANAN_BIN}/
	DEPENDS sync
	USES_TERMINAL
)
