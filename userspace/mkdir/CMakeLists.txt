set(SOURCES
	main.cpp
)

add_executable(mkdir ${SOURCES})
banan_link_library(mkdir libc)

install(TARGETS mkdir)
