set(SOURCES
	main.cpp
)

add_executable(chmod ${SOURCES})
banan_link_library(chmod libc)

install(TARGETS chmod)
