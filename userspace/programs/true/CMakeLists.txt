set(SOURCES
	main.cpp
)

add_executable(true ${SOURCES})
banan_link_library(true ban)
banan_link_library(true libc)

install(TARGETS true OPTIONAL)
