set(SOURCES
	main.cpp
)

add_executable(cat-mmap ${SOURCES})
banan_link_library(cat-mmap libc)

install(TARGETS cat-mmap OPTIONAL)
