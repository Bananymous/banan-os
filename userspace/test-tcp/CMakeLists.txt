set(SOURCES
	main.cpp
)

add_executable(test-tcp ${SOURCES})
banan_link_library(test-tcp libc)

install(TARGETS test-tcp)
