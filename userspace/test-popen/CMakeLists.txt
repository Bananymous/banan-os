set(SOURCES
	main.cpp
)

add_executable(test-popen ${SOURCES})
banan_link_library(test-popen libc)

install(TARGETS test-popen)
