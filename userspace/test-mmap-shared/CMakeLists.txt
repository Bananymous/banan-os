set(SOURCES
	main.cpp
)

add_executable(test-mmap-shared ${SOURCES})
banan_link_library(test-mmap-shared libc)

install(TARGETS test-mmap-shared OPTIONAL)
