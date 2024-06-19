set(SOURCES
	main.cpp
)

add_executable(sync ${SOURCES})
banan_link_library(sync libc)

install(TARGETS sync)
