set(SOURCES
	main.cpp
)

add_executable(stat ${SOURCES})
banan_link_library(stat ban)
banan_link_library(stat libc)

install(TARGETS stat OPTIONAL)
