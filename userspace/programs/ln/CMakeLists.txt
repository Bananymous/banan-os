set(SOURCES
	main.cpp
)

add_executable(ln ${SOURCES})
banan_link_library(ln libc)

install(TARGETS ln OPTIONAL)
