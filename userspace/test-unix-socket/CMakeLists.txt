set(SOURCES
	main.cpp
)

add_executable(test-unix-socket ${SOURCES})
banan_link_library(test-unix-socket libc)

install(TARGETS test-unix-socket)
