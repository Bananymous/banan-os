set(SOURCES
	main.cpp
)

add_executable(loadfont ${SOURCES})
banan_link_library(loadfont ban)
banan_link_library(loadfont libc)

install(TARGETS loadfont)
