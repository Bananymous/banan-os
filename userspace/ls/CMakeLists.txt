set(SOURCES
	main.cpp
)

add_executable(ls ${SOURCES})
banan_link_library(ls ban)
banan_link_library(ls libc)

install(TARGETS ls)
