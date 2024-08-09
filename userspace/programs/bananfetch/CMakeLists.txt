set(SOURCES
	main.cpp
)

add_executable(bananfetch ${SOURCES})
banan_link_library(bananfetch ban)
banan_link_library(bananfetch libc)

install(TARGETS bananfetch OPTIONAL)
