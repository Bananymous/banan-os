set(SOURCES
	main.cpp
)

add_executable(test-udp ${SOURCES})
banan_link_library(test-udp libc)

install(TARGETS test-udp)
