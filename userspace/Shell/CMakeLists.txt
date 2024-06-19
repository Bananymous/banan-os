set(SOURCES
	main.cpp
)

add_executable(Shell ${SOURCES})
banan_link_library(Shell ban)
banan_link_library(Shell libc)

install(TARGETS Shell)
