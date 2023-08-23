cmake_minimum_required(VERSION 3.26)

project(yes CXX)

set(SOURCES
	main.cpp
)

add_executable(yes ${SOURCES})
target_compile_options(yes PUBLIC -O2 -g)
target_link_libraries(yes PUBLIC libc)

add_custom_target(yes-install
	COMMAND sudo cp ${CMAKE_CURRENT_BINARY_DIR}/yes ${BANAN_BIN}/
	DEPENDS yes
	USES_TERMINAL
)
